import React, { useCallback, useEffect, useRef, useState } from 'react';
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Alert,
  AlertTitle,
  Box,
  Button,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  LinearProgress,
  Link,
  Step,
  StepContent,
  StepLabel,
  Stepper,
  Typography,
} from '@mui/material';
import { Download, ExpandMore } from '@mui/icons-material';
import FileSaver from 'file-saver';
import { FormValues } from '../types';
import { newIssueLink } from '../utils';
import { BuildKeyPair } from '../crypto';
import {
  UndecryptableArchiveError,
  decryptFirmwareArchive,
} from '../firmware/archiveCrypto';
import {
  BuildStatus,
  BuildStep,
  BuildTimeoutError,
  buildListUrl,
  fetchLogSegment,
  firmwareDownloadUrl,
  pollBuildStatus,
} from '../firmware/buildStatus';
import {
  FirmwareBundle,
  downloadFirmwareArchive,
  extractFirmwareBundle,
} from '../firmware/firmwareBundle';
import EspWebInstallButton, {
  isFlashingSupported,
} from './EspWebInstallButton';

type Phase = 'building' | 'preparing' | 'ready' | 'error';

interface BuildProgressDialogProps {
  identifier: string;
  keyPair: BuildKeyPair;
  deviceName: string;
  config: FormValues;
  onClose: () => void;
}

const STEP_LABELS: Record<BuildStep, string> = {
  preparing: 'Preparing the build',
  compiling: 'Compiling the firmware',
  packaging: 'Packaging the firmware',
};

const stepLabel = (status: BuildStatus | null): string => {
  if (!status) {
    return 'Waiting for the build to start';
  }
  return (
    (status.step && STEP_LABELS[status.step]) ??
    status.message ??
    'Build running'
  );
};

/** How much of the compile is done, or null while that is unknown. */
const percentComplete = (status: BuildStatus | null): number | null => {
  const progress = status?.progress;
  if (!progress?.total) {
    return null;
  }
  // Never show a full bar: the build is only done once the status says so.
  return Math.min(99, (progress.completed / progress.total) * 100);
};

const compiledFiles = (status: BuildStatus | null): string | null => {
  const progress = status?.progress;
  if (!progress) {
    return null;
  }
  return progress.total
    ? `${progress.completed} of ${progress.total} files`
    : `${progress.completed} files`;
};

/**
 * The tail of the build output, pinned to its last line.
 *
 * Anchored only while it is already at the bottom, so a user who scrolled up to
 * read an error is not yanked back down by the next update - which arrives
 * every few seconds for the length of the compile.
 */
const BuildLog: React.FC<{ log: string }> = ({ log }) => {
  const box = useRef<HTMLElement | null>(null);
  const isPinned = useRef(true);

  useEffect(() => {
    const node = box.current;
    if (node && isPinned.current) {
      node.scrollTop = node.scrollHeight;
    }
  }, [log]);

  return (
    <Box
      component="pre"
      ref={box}
      onScroll={(event: React.UIEvent<HTMLElement>) => {
        const node = event.currentTarget;
        isPinned.current =
          node.scrollHeight - node.scrollTop - node.clientHeight < 24;
      }}
      sx={{
        m: 0,
        p: 1,
        maxHeight: 220,
        overflow: 'auto',
        bgcolor: 'action.hover',
        borderRadius: 1,
        fontFamily: 'monospace',
        fontSize: '0.72rem',
        lineHeight: 1.5,
        whiteSpace: 'pre-wrap',
        overflowWrap: 'anywhere',
      }}
    >
      {log}
    </Box>
  );
};

const formatDuration = (seconds: number) => {
  const minutes = Math.floor(seconds / 60);
  const remainder = Math.floor(seconds % 60);
  return `${minutes}:${remainder.toString().padStart(2, '0')}`;
};

const errorMessage = (error: unknown): string => {
  if (error instanceof BuildTimeoutError) {
    return (
      'The build did not finish in time. It might still be running - check the ' +
      'build log, then use "Try again" to pick the firmware up. Do not reload ' +
      'this page: the key that decrypts it only exists here.'
    );
  }
  if (error instanceof TypeError) {
    // fetch() rejects with a TypeError when the request never made it through,
    // e.g. when it was blocked by CORS or the network. The published object is
    // encrypted, so there is no useful manual route to fall back to here.
    return (
      'The firmware could not be downloaded in this browser. Check your ' +
      'network connection and try again.'
    );
  }
  if (error instanceof Error) {
    return error.message;
  }
  return 'Something went wrong while preparing your firmware.';
};

const BuildProgressDialog: React.FC<BuildProgressDialogProps> = ({
  identifier,
  keyPair,
  deviceName,
  config,
  onClose,
}) => {
  const [phase, setPhase] = useState<Phase>('building');
  const [status, setStatus] = useState<BuildStatus | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [isRetryable, setIsRetryable] = useState(false);
  // Whether the build itself failed, as opposed to this page failing to turn a
  // finished build into something flashable. Not the same as `!isRetryable`:
  // an archive this page cannot decrypt is a successful build we cannot use.
  const [didBuildFail, setDidBuildFail] = useState(false);
  const [progress, setProgress] = useState<number | null>(null);
  const [bundle, setBundle] = useState<FirmwareBundle | null>(null);
  const [archive, setArchive] = useState<Blob | null>(null);
  const [elapsedSeconds, setElapsedSeconds] = useState(0);
  const [attempt, setAttempt] = useState(0);
  const [isStatusUnreachable, setIsStatusUnreachable] = useState(false);
  // Accumulated from the log segments, so it only ever grows. A build that
  // loses access to its own log mid-compile keeps what it already showed.
  const [buildLog, setBuildLog] = useState<string | null>(null);

  useEffect(() => {
    const controller = new AbortController();
    let created: FirmwareBundle | null = null;

    const run = async () => {
      setPhase('building');
      setError(null);
      setProgress(null);
      setArchive(null);
      setDidBuildFail(false);
      setIsStatusUnreachable(false);
      setBuildLog(null);

      // Segments are downloaded once each, in order, as the status document
      // announces them. Chained rather than guarded by a flag so that the last
      // call can be awaited: a failed build's error is usually in the segment
      // the workflow published just before it said the build had failed.
      let cursor = 0;
      let pumping: Promise<void> = Promise.resolve();
      const pumpLog = (available: number) => {
        pumping = pumping.then(async () => {
          while (cursor < available && !controller.signal.aborted) {
            let segment: string | null = null;
            try {
              segment = await fetchLogSegment(
                identifier,
                cursor,
                controller.signal
              );
            } catch (caught) {
              return; // Transient, or aborted. The next status brings us back.
            }
            if (segment === null) {
              return; // Announced but not readable yet.
            }
            cursor += 1;
            if (segment.length > 0) {
              setBuildLog((previous) => (previous ?? '') + segment);
            }
          }
        });
        return pumping;
      };

      try {
        const finalStatus = await pollBuildStatus({
          identifier,
          signal: controller.signal,
          onStatus: (update) => {
            setStatus(update);
            setIsStatusUnreachable(false);
            if (update.logSegments) {
              void pumpLog(update.logSegments);
            }
          },
          // A handful of failures in a row usually means the browser blocked
          // the request, e.g. because the bucket is missing a CORS rule.
          onFetchError: (_error, consecutiveFailures) =>
            setIsStatusUnreachable(consecutiveFailures >= 3),
        });
        // Before anything is decided about the build, so that a failure is
        // shown with everything the compiler said rather than most of it.
        await pumpLog(finalStatus.logSegments ?? 0);
        if (finalStatus.state === 'error') {
          setIsRetryable(false);
          setDidBuildFail(true);
          setError(
            finalStatus.message ??
              'The firmware build failed. Please check the build log for details.'
          );
          setPhase('error');
          return;
        }

        setPhase('preparing');
        const encrypted = await downloadFirmwareArchive(
          finalStatus.firmwareUrl ?? firmwareDownloadUrl(identifier),
          { signal: controller.signal, onProgress: setProgress }
        );
        const archive = await decryptFirmwareArchive(
          encrypted,
          keyPair.privateKey
        );
        // Kept so the manual route can hand over a ZIP that opens anywhere.
        setArchive(archive);
        const extracted = await extractFirmwareBundle(archive, {
          name: deviceName,
          version: finalStatus.esphomeVersion ?? 'ESPHome',
        });
        if (controller.signal.aborted) {
          extracted.release();
          return;
        }
        created = extracted;
        setBundle(extracted);
        setPhase('ready');
      } catch (caught) {
        if (controller.signal.aborted) {
          return;
        }
        // Retrying re-downloads the same object and derives the same key, so an
        // archive that does not belong to this build never will.
        setIsRetryable(!(caught instanceof UndecryptableArchiveError));
        setError(errorMessage(caught));
        setPhase('error');
      }
    };

    run();

    return () => {
      controller.abort();
      created?.release();
      setBundle(null);
      setArchive(null);
    };
  }, [identifier, keyPair, deviceName, attempt]);

  useEffect(() => {
    if (phase !== 'building') {
      return;
    }
    const startedAt = Date.now();
    const timer = setInterval(
      () => setElapsedSeconds(Math.floor((Date.now() - startedAt) / 1000)),
      1000
    );
    return () => clearInterval(timer);
  }, [phase, attempt]);

  const handleClose = useCallback(() => {
    if (phase === 'ready' || phase === 'error') {
      onClose();
      return;
    }
    const confirmed = window.confirm(
      'The build is still running. The key that decrypts your firmware only ' +
        'exists in this dialog, so closing it means starting the build again. ' +
        'Close anyway?'
    );
    if (confirmed) {
      onClose();
    }
  }, [phase, onClose]);

  const handleArchiveDownload = () => {
    if (archive) {
      FileSaver.saveAs(archive, `${identifier}.zip`);
    }
  };

  const runLink = status?.runUrl ?? buildListUrl;
  const buildPercent = percentComplete(status);
  const buildFiles = compiledFiles(status);
  const activeStep = phase === 'building' ? 0 : phase === 'preparing' ? 1 : 2;

  const manualInstructions = (
    <Accordion sx={{ mt: 2 }} disableGutters>
      <AccordionSummary expandIcon={<ExpandMore />}>
        <Typography variant="body2">
          Flash manually instead (ESPHome Web Installer)
        </Typography>
      </AccordionSummary>
      <AccordionDetails>
        <Typography variant="body2" component="div">
          <ol style={{ paddingLeft: '1.2em', margin: 0 }}>
            <li>
              {archive ? (
                <>
                  Save the firmware to your computer and unzip it:
                  <Box sx={{ my: 1 }}>
                    <Button
                      variant="outlined"
                      size="small"
                      startIcon={<Download />}
                      onClick={handleArchiveDownload}
                    >
                      Download {identifier}.zip
                    </Button>
                  </Box>
                </>
              ) : (
                <>
                  Wait for the build to finish - this page decrypts the firmware
                  and offers it here as a ZIP you can save and unzip with any
                  tool.
                </>
              )}
            </li>
            <li>Connect your ESP32 to your computer.</li>
            <li>
              Open{' '}
              <Link
                href="https://web.esphome.io/"
                target="_blank"
                rel="noopener"
              >
                ESPHome Web
              </Link>
              , connect to your ESP32, click "Install" and select the extracted{' '}
              <code>*.factory.bin</code> file.
            </li>
          </ol>
        </Typography>
      </AccordionDetails>
    </Accordion>
  );

  return (
    <Dialog open={true} onClose={handleClose} fullWidth maxWidth="sm">
      <DialogTitle>Building &amp; Flashing</DialogTitle>
      <DialogContent dividers>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Build <strong>{identifier}</strong>
        </Typography>

        {phase === 'error' ? (
          <Alert severity="error" sx={{ mt: 1 }}>
            <AlertTitle>
              {didBuildFail ? 'Build failed' : 'Could not prepare the firmware'}
            </AlertTitle>
            {error}
            {buildLog && (
              <Box sx={{ mt: 1 }}>
                <Typography variant="caption" color="text.secondary">
                  Build output
                </Typography>
                <BuildLog log={buildLog} />
              </Box>
            )}
            <Box sx={{ mt: 1 }}>
              <Link href={runLink} target="_blank" rel="noopener">
                View build log
              </Link>
              {' · '}
              <Link
                href={newIssueLink({ config, build: identifier })}
                target="_blank"
                rel="noopener"
              >
                Report an issue
              </Link>
            </Box>
          </Alert>
        ) : (
          <Stepper activeStep={activeStep} orientation="vertical">
            <Step>
              <StepLabel>Building your firmware</StepLabel>
              <StepContent>
                <Typography variant="body2" gutterBottom>
                  GitHub Actions is building your configuration. This usually
                  takes around five minutes - keep this dialog open.
                </Typography>
                <LinearProgress
                  sx={{ my: 1 }}
                  variant={
                    buildPercent === null ? 'indeterminate' : 'determinate'
                  }
                  value={buildPercent ?? undefined}
                />
                {isStatusUnreachable && (
                  <Alert severity="warning" sx={{ my: 1 }}>
                    We cannot read the build status from this browser. Your
                    build is most likely still running - follow it in the build
                    log and leave this page open. The key that decrypts your
                    firmware only exists here, so reloading means starting the
                    build again.
                  </Alert>
                )}
                <Typography variant="caption" color="text.secondary">
                  {stepLabel(status)}
                  {buildFiles ? ` · ${buildFiles}` : ''} ·{' '}
                  {formatDuration(elapsedSeconds)} ·{' '}
                  <Link href={runLink} target="_blank" rel="noopener">
                    View build log
                  </Link>
                </Typography>
                {buildLog && (
                  <Accordion sx={{ mt: 1 }} disableGutters defaultExpanded>
                    <AccordionSummary expandIcon={<ExpandMore />}>
                      <Typography variant="body2">Build output</Typography>
                    </AccordionSummary>
                    <AccordionDetails sx={{ p: 1, pt: 0 }}>
                      <BuildLog log={buildLog} />
                    </AccordionDetails>
                  </Accordion>
                )}
              </StepContent>
            </Step>
            <Step>
              <StepLabel>Downloading the firmware</StepLabel>
              <StepContent>
                <Typography variant="body2" gutterBottom>
                  Downloading and decrypting the firmware in your browser.
                </Typography>
                <LinearProgress
                  sx={{ my: 1 }}
                  variant={progress === null ? 'indeterminate' : 'determinate'}
                  value={progress === null ? undefined : progress * 100}
                />
              </StepContent>
            </Step>
            <Step>
              <StepLabel>Install on your ESP32</StepLabel>
              <StepContent>
                {isFlashingSupported() ? (
                  <>
                    <Typography variant="body2" gutterBottom>
                      Connect your ESP32 via USB, then click the button below
                      and pick its serial port.
                    </Typography>
                    {bundle && (
                      <Box sx={{ my: 1 }}>
                        <EspWebInstallButton manifestUrl={bundle.manifestUrl} />
                      </Box>
                    )}
                    {bundle && (
                      <Typography variant="caption" color="text.secondary">
                        Firmware for {bundle.chipFamily}
                        {status?.esphomeVersion
                          ? ` · ESPHome ${status.esphomeVersion}`
                          : ''}
                      </Typography>
                    )}
                  </>
                ) : (
                  <Alert severity="info">
                    <AlertTitle>Browser not supported</AlertTitle>
                    Flashing from the browser requires the Web Serial API. Open
                    this page in Google Chrome, Microsoft Edge or Opera on a
                    desktop computer, or use the manual instructions below.
                  </Alert>
                )}
              </StepContent>
            </Step>
          </Stepper>
        )}

        {manualInstructions}
      </DialogContent>
      <DialogActions>
        {phase === 'error' && isRetryable && (
          <Button onClick={() => setAttempt((value) => value + 1)}>
            Try again
          </Button>
        )}
        <Button onClick={handleClose} color="primary">
          Close
        </Button>
      </DialogActions>
    </Dialog>
  );
};

export default BuildProgressDialog;
