import React, { useCallback, useEffect, useState } from 'react';
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
import { ContentCopy, ExpandMore } from '@mui/icons-material';
import { FormValues } from '../types';
import { newIssueLink } from '../utils';
import {
  BuildStatus,
  BuildTimeoutError,
  buildListUrl,
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
  password: string;
  deviceName: string;
  config: FormValues;
  onClose: () => void;
}

const formatDuration = (seconds: number) => {
  const minutes = Math.floor(seconds / 60);
  const remainder = Math.floor(seconds % 60);
  return `${minutes}:${remainder.toString().padStart(2, '0')}`;
};

const errorMessage = (error: unknown): string => {
  if (error instanceof BuildTimeoutError) {
    return (
      'The build did not finish in time. It might still be running - check the ' +
      'build log and use the manual instructions below once it succeeded.'
    );
  }
  if (error instanceof TypeError) {
    // fetch() rejects with a TypeError when the request never made it through,
    // e.g. when it was blocked by CORS or the network.
    return (
      'The firmware could not be downloaded in this browser. Please use the ' +
      'manual instructions below.'
    );
  }
  if (error instanceof Error) {
    return error.message;
  }
  return 'Something went wrong while preparing your firmware.';
};

const BuildProgressDialog: React.FC<BuildProgressDialogProps> = ({
  identifier,
  password,
  deviceName,
  config,
  onClose,
}) => {
  const [phase, setPhase] = useState<Phase>('building');
  const [status, setStatus] = useState<BuildStatus | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [isRetryable, setIsRetryable] = useState(false);
  const [progress, setProgress] = useState<number | null>(null);
  const [bundle, setBundle] = useState<FirmwareBundle | null>(null);
  const [elapsedSeconds, setElapsedSeconds] = useState(0);
  const [attempt, setAttempt] = useState(0);
  const [isStatusUnreachable, setIsStatusUnreachable] = useState(false);

  useEffect(() => {
    const controller = new AbortController();
    let created: FirmwareBundle | null = null;

    const run = async () => {
      setPhase('building');
      setError(null);
      setProgress(null);
      setIsStatusUnreachable(false);
      try {
        const finalStatus = await pollBuildStatus({
          identifier,
          signal: controller.signal,
          onStatus: (update) => {
            setStatus(update);
            setIsStatusUnreachable(false);
          },
          // A handful of failures in a row usually means the browser blocked
          // the request, e.g. because the bucket is missing a CORS rule.
          onFetchError: (_error, consecutiveFailures) =>
            setIsStatusUnreachable(consecutiveFailures >= 3),
        });
        if (finalStatus.state === 'error') {
          setIsRetryable(false);
          setError(
            finalStatus.message ??
              'The firmware build failed. Please check the build log for details.'
          );
          setPhase('error');
          return;
        }

        setPhase('preparing');
        const archive = await downloadFirmwareArchive(
          finalStatus.firmwareUrl ?? firmwareDownloadUrl(identifier),
          { signal: controller.signal, onProgress: setProgress }
        );
        const extracted = await extractFirmwareBundle(archive, password, {
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
        setIsRetryable(true);
        setError(errorMessage(caught));
        setPhase('error');
      }
    };

    run();

    return () => {
      controller.abort();
      created?.release();
      setBundle(null);
    };
  }, [identifier, password, deviceName, attempt]);

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
      'The build is still running. If you close this dialog you have to ' +
        'download and flash the firmware manually. Close anyway?'
    );
    if (confirmed) {
      onClose();
    }
  }, [phase, onClose]);

  const handlePasswordCopy = () => {
    // Optional: navigator.clipboard is undefined in insecure contexts.
    navigator.clipboard?.writeText(password).catch(() => {
      // Clipboard access can be denied, the password is visible anyway.
    });
  };

  const runLink = status?.runUrl ?? buildListUrl;
  const downloadUrl = status?.firmwareUrl ?? firmwareDownloadUrl(identifier);
  const isBuilt = phase === 'ready' || (phase === 'error' && isRetryable);
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
              {isBuilt ? (
                <>
                  Download the firmware:{' '}
                  <Link href={downloadUrl}>{identifier}.zip</Link>
                </>
              ) : (
                <>
                  Wait for the build to finish, then download the firmware from{' '}
                  <Link href={downloadUrl}>{identifier}.zip</Link>
                </>
              )}
            </li>
            <li>
              Unzip it using this password:{' '}
              <Box
                component="strong"
                onClick={handlePasswordCopy}
                sx={{ cursor: 'pointer', whiteSpace: 'nowrap' }}
                title="Copy to clipboard"
              >
                {password} <ContentCopy fontSize="inherit" />
              </Box>
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
              {isRetryable ? 'Could not prepare the firmware' : 'Build failed'}
            </AlertTitle>
            {error}
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
                  GitHub Actions is compiling your configuration. This usually
                  takes 3 to 10 minutes - keep this dialog open.
                </Typography>
                <LinearProgress sx={{ my: 1 }} />
                {isStatusUnreachable && (
                  <Alert severity="warning" sx={{ my: 1 }}>
                    We cannot read the build status from this browser. Your
                    build is most likely still running - follow it in the build
                    log and use the manual instructions below once it finished.
                  </Alert>
                )}
                <Typography variant="caption" color="text.secondary">
                  {status?.state === 'building'
                    ? 'Build running'
                    : 'Waiting for the build to start'}{' '}
                  · {formatDuration(elapsedSeconds)} ·{' '}
                  <Link href={runLink} target="_blank" rel="noopener">
                    View build log
                  </Link>
                </Typography>
              </StepContent>
            </Step>
            <Step>
              <StepLabel>Preparing the firmware</StepLabel>
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
