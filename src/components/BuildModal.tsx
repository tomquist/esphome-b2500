import React, { useState } from 'react';
import {
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  Button,
  CircularProgress,
} from '@mui/material';
import axios from 'axios';
import {
  getAllSecrets,
  generateRandomIdentifier,
  generatePassword,
} from '../utils';
import { encryptConfig, encryptPassword } from '../crypto';
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
  const [password] = useState(() => generatePassword());
  const [isLoading, setIsLoading] = useState(false);

  const handleBuild = async () => {
    if (!debouncedFormValues) return;
    setIsLoading(true);

    const encryptedPassword = encryptPassword(password);
    const configData = {
      secrets: getAllSecrets(debouncedFormValues),
      config: debouncedFormValues,
    };
    const encryptedConfig = encryptConfig(configData, password);

    try {
      await axios.post(
        'https://publicactiontrigger.azurewebsites.net/api/dispatches/tomquist/esphome-b2500',
        {
          event_type: 'build_esphome',
          client_payload: {
            config: encryptedConfig,
            identifier,
            password: encryptedPassword,
          },
        },
        {
          headers: {
            'Content-Type': 'application/json',
          },
        }
      );
      setIsBuildStarted(true);
    } catch (error) {
      console.error('Error triggering build:', error);
      alert('Failed to trigger build. Check the console for more details.');
    } finally {
      setIsLoading(false);
    }
  };

  if (isBuildStarted) {
    return (
      <BuildProgressDialog
        identifier={identifier}
        password={password}
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
        </DialogContentText>
      </DialogContent>
      <DialogActions>
        <Button onClick={closeModal} color="primary" disabled={isLoading}>
          Cancel
        </Button>
        <Button
          onClick={handleBuild}
          color="primary"
          disabled={
            isLoading || password.length < 8 || identifier.trim().length === 0
          }
        >
          {isLoading ? <CircularProgress size={24} /> : 'Start Build'}
        </Button>
      </DialogActions>
    </Dialog>
  );
};

export default BuildModal;
