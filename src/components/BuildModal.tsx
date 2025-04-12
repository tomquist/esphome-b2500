import React, { useState } from 'react';
import {
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  Button,
  CircularProgress,
  Link,
} from '@mui/material';
import { ContentCopy } from '@mui/icons-material';
import axios from 'axios';
import {
  getAllSecrets,
  generateRandomIdentifier,
  generatePassword,
  newIssueLink,
} from '../utils';
import { encryptConfig, encryptPassword } from '../crypto';

const S3_BUCKET = process.env.REACT_APP_S3_BUCKET;
const AWS_REGION = process.env.REACT_APP_AWS_REGION;

interface BuildModalProps {
  closeModal: () => void;
  debouncedFormValues: any;
}

const BuildModal: React.FC<BuildModalProps> = ({
  closeModal,
  debouncedFormValues,
}) => {
  const [isInstructionsModalOpen, setIsInstructionsModalOpen] = useState(false);
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
      navigator.clipboard.writeText(password);
      setIsInstructionsModalOpen(true);
    } catch (error) {
      console.error('Error triggering build:', error);
      alert('Failed to trigger build. Check the console for more details.');
    } finally {
      setIsLoading(false);
    }
  };

  const handlePasswordCopy = () => {
    navigator.clipboard.writeText(password);
    alert('Password copied to clipboard');
  };

  const handleCloseInstructionsModal = () => {
    setIsInstructionsModalOpen(false);
    closeModal();
  };

  return (
    <>
      <Dialog open={true} onClose={closeModal} disableEscapeKeyDown={isLoading}>
        <DialogTitle>Build Image</DialogTitle>
        <DialogContent>
          <DialogContentText>
            <p>
              This allows you to directly build the firmware image on GitHub
              Actions.
            </p>
            <p>
              Once you press "Start Build", we'll <strong>encrypt</strong> your
              configuration and send it over to GitHub to build the ESPHome
              image. Once the build is complete, you can download a password
              protected Zip-file from the "Artifacts" section of your build.
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

      <Dialog
        open={isInstructionsModalOpen}
        onClose={handleCloseInstructionsModal}
      >
        <DialogTitle>Next Steps</DialogTitle>
        <DialogContent>
          <DialogContentText>
            Your build has been started now. Here's what you need to do next:
            <ol>
              <li>
                Open the list of builds:{' '}
                <Link
                  href="https://github.com/tomquist/esphome-b2500/actions"
                  target="_blank"
                  rel="noopener"
                >
                  Build List
                </Link>{' '}
                and locate your build: <strong>[B2500] {identifier}</strong>.
                Keep this page open for the next steps!
              </li>
              <li>
                Wait for the build to finish. If the build fails, please{' '}
                <Link
                  href={newIssueLink({
                    config: debouncedFormValues,
                    build: identifier,
                  })}
                  target="_blank"
                  rel="noopener"
                >
                  open an issue
                </Link>{' '}
                with details of your configuration.
              </li>
              <li>
                Once the build completed, download your firmware using this
                link:{' '}
                <Link
                  href={`https://${S3_BUCKET}.s3.${AWS_REGION}.amazonaws.com/firmware/${identifier}.zip`}
                >
                  Firmware Download
                </Link>
              </li>
              <li>
                Unzip the file using the entered password:
                <strong
                  onClick={handlePasswordCopy}
                  style={{ cursor: 'pointer', marginLeft: '5px' }}
                >
                  {password} <ContentCopy />
                </strong>
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
                , connect to your ESP32, click "Install" and select the unzipped
                <code>*.bin</code> file.
              </li>
            </ol>
          </DialogContentText>
        </DialogContent>
        <DialogActions>
          <Button onClick={handleCloseInstructionsModal} color="primary">
            Close
          </Button>
        </DialogActions>
      </Dialog>
    </>
  );
};

export default BuildModal;
