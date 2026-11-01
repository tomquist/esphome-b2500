import React, { useState } from 'react';
import {
  Alert,
  AlertTitle,
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  Button,
  CircularProgress,
} from '@mui/material';
import axios, { isAxiosError } from 'axios';
import { getAllSecrets, generateRandomIdentifier } from '../utils';
import {
  BuildKeyPair,
  buildKeyIsConfigured,
  encryptConfig,
  generateBuildKeyPair,
} from '../crypto';
import BuildProgressDialog from './BuildProgressDialog';
import { FormValues } from '../types';

/**
 * Where builds are started: the function in infra/dispatcher, which turns the
 * request into a run of the build workflow. Deployment configuration like the
 * build public key, so moving the endpoint needs no code change.
 */
const dispatchUrl = process.env.REACT_APP_BUILD_DISPATCH_URL ?? '';

const buildsAreConfigured = () => buildKeyIsConfigured() && dispatchUrl !== '';

/** What to tell the user when the dispatcher did not start the build. */
const describeDispatchError = (error: unknown): string => {
  if (isAxiosError(error)) {
    if (!error.response) {
      return 'The build service could not be reached. Please try again later. In the meantime you can download the YAML and build it with your own ESPHome installation.';
    }
    const reason = (error.response.data as { error?: unknown } | undefined)
      ?.error;
    if (typeof reason === 'string') {
      // A 4xx is about this request, so retrying will not help; a 5xx is the
      // service or GitHub, which retrying later might.
      return error.response.status < 500
        ? `The build service refused the build: ${reason}. Please open an issue.`
        : `${reason}. Please try again later.`;
    }
    return `The build service answered with HTTP ${error.response.status}. Please try again later.`;
  }
  return 'The build could not be started. Check the browser console for details.';
};

interface BuildModalProps {
  closeModal: () => void;
  debouncedFormValues: FormValues;
}

const BuildModal: React.FC<BuildModalProps> = ({
  closeModal,
  debouncedFormValues,
}) => {
  // The identifier and key pair of the dispatch that went out. Both are
  // generated per attempt: a dispatch reported as failed may still have
  // started a build, and a retry under the same identifier would have the
  // progress dialog follow that build, whose firmware is sealed to the
  // previous attempt's key. The private key is never transmitted: the payload
  // carries only the public half, and the firmware comes back sealed to it.
  const [build, setBuild] = useState<{
    identifier: string;
    keyPair: BuildKeyPair;
  } | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [dispatchError, setDispatchError] = useState<string | null>(null);

  const handleBuild = async () => {
    if (!debouncedFormValues) return;
    setIsLoading(true);
    setDispatchError(null);

    try {
      const identifier = generateRandomIdentifier();
      const buildKeyPair = await generateBuildKeyPair();
      const configData = {
        secrets: getAllSecrets(debouncedFormValues),
        config: debouncedFormValues,
      };
      const encryptedConfig = await encryptConfig(configData, buildKeyPair);

      await axios.post(
        dispatchUrl,
        {
          identifier,
          config: encryptedConfig,
          public_key: buildKeyPair.publicKey,
        },
        {
          headers: {
            'Content-Type': 'application/json',
          },
        }
      );
      // Only kept once the dispatch went out: without a build to match it, the
      // key pair is of no use to the progress dialog.
      setBuild({ identifier, keyPair: buildKeyPair });
    } catch (error) {
      console.error('Error triggering build:', error);
      setDispatchError(describeDispatchError(error));
    } finally {
      setIsLoading(false);
    }
  };

  if (build) {
    return (
      <BuildProgressDialog
        identifier={build.identifier}
        keyPair={build.keyPair}
        deviceName={
          debouncedFormValues.friendly_name || debouncedFormValues.name
        }
        config={debouncedFormValues}
        onClose={closeModal}
      />
    );
  }

  return (
    <Dialog open={true} onClose={closeModal} disableEscapeKeyDown={isLoading}>
      <DialogTitle>Build Image</DialogTitle>
      <DialogContent>
        <DialogContentText component="div">
          <p>
            This builds your firmware image on GitHub Actions and then installs
            it on your ESP32 directly from this page.
          </p>
          <p>
            Once you press "Start Build", we'll <strong>encrypt</strong> your
            configuration and send it over to GitHub. When the build is done,
            this page downloads the firmware, decrypts it in your browser and
            offers to flash it via USB. Flashing requires a Chromium based
            browser such as Google Chrome or Microsoft Edge - you can always
            download the firmware and flash it manually instead.
          </p>
          <p>
            The key that unlocks your firmware is generated here and never
            leaves this page, so keep it open until the build finishes.
          </p>
        </DialogContentText>
        {!buildsAreConfigured() && (
          <Alert severity="error">
            <AlertTitle>Builds are not configured</AlertTitle>
            This deployment is missing its build public key or the address of
            the build service, so builds cannot be started from here. Please
            open an issue.
          </Alert>
        )}
        {dispatchError && (
          <Alert severity="error">
            <AlertTitle>The build did not start</AlertTitle>
            {dispatchError}
          </Alert>
        )}
      </DialogContent>
      <DialogActions>
        <Button onClick={closeModal} color="primary" disabled={isLoading}>
          Cancel
        </Button>
        <Button
          onClick={handleBuild}
          color="primary"
          disabled={isLoading || !buildsAreConfigured()}
        >
          {isLoading ? <CircularProgress size={24} /> : 'Start Build'}
        </Button>
      </DialogActions>
    </Dialog>
  );
};

export default BuildModal;
