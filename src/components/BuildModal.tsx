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
import axios from 'axios';
import { getAllSecrets, generateRandomIdentifier } from '../utils';
import {
  BuildKeyPair,
  buildKeyIsConfigured,
  encryptConfig,
  generateBuildKeyPair,
} from '../crypto';
import BuildProgressDialog from './BuildProgressDialog';
import { FormValues } from '../types';

interface BuildModalProps {
  closeModal: () => void;
  debouncedFormValues: FormValues;
}

const BuildModal: React.FC<BuildModalProps> = ({
  closeModal,
  debouncedFormValues,
}) => {
  const [isBuildStarted, setIsBuildStarted] = useState(false);
  const [identifier] = useState(() => generateRandomIdentifier());
  // Generated per build and never transmitted: the payload carries only the
  // public half, and the firmware comes back sealed to it.
  const [keyPair, setKeyPair] = useState<BuildKeyPair | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  const handleBuild = async () => {
    if (!debouncedFormValues) return;
    setIsLoading(true);

    try {
      const buildKeyPair = await generateBuildKeyPair();
      const configData = {
        secrets: getAllSecrets(debouncedFormValues),
        config: debouncedFormValues,
      };
      const encryptedConfig = await encryptConfig(configData, buildKeyPair);

      await axios.post(
        'https://publicactiontrigger.azurewebsites.net/api/dispatches/tomquist/esphome-b2500',
        {
          event_type: 'build_esphome',
          client_payload: {
            config: encryptedConfig,
            identifier,
            public_key: buildKeyPair.publicKey,
          },
        },
        {
          headers: {
            'Content-Type': 'application/json',
          },
        }
      );
      // Only kept once the dispatch went out: without a build to match it, the
      // key pair is of no use to the progress dialog.
      setKeyPair(buildKeyPair);
      setIsBuildStarted(true);
    } catch (error) {
      console.error('Error triggering build:', error);
      alert('Failed to trigger build. Check the console for more details.');
    } finally {
      setIsLoading(false);
    }
  };

  if (isBuildStarted && keyPair) {
    return (
      <BuildProgressDialog
        identifier={identifier}
        keyPair={keyPair}
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
        {!buildKeyIsConfigured() && (
          <Alert severity="error">
            <AlertTitle>Builds are not configured</AlertTitle>
            This deployment is missing its build public key, so a configuration
            sent from here could not be decrypted. Please open an issue.
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
          disabled={
            isLoading ||
            !buildKeyIsConfigured() ||
            identifier.trim().length === 0
          }
        >
          {isLoading ? <CircularProgress size={24} /> : 'Start Build'}
        </Button>
      </DialogActions>
    </Dialog>
  );
};

export default BuildModal;
