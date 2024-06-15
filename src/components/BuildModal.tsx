import React, { useState } from 'react';
import {
    Dialog,
    DialogActions,
    DialogContent,
    DialogContentText,
    DialogTitle,
    Button,
    CircularProgress,
    Link, Box,
} from '@mui/material';
import { ContentCopy } from '@mui/icons-material';
import axios from 'axios';
import crypto from 'crypto';
import {getAllSecrets, aesEncrypt, generateRandomIdentifier, generatePassword, newIssueLink} from '../utils';

const publicKey = `-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvvRsA62G+HxFVteRvo9R
at3tt1hLzO1C//n0PgE0ljrc4Y5p+xfIrMe9lgo0Y0+/I5xtpOyzI6cCGiQfmpH8
v1/ZPwRJjG5Oezj910p7B90Z+bg2lS1H2BBPEq2CiDbUCtcd5RKZBiVv9Yjmwm/J
9DOGfiAB8lgmVReqe5fAc8IiZUnljp01LdCoxesp6HKSSEalet1pydj0joTR2xWy
80m58r69liFdLiZBzXYIlbpJRt91KSUyFEeRginm6mkrEjzu8bPKiZlXA3ZvbcSa
JSumPOJxYkcpse3uabXIEYRV3CL35R6CQN9YEYDYoGBrrQeAO5xgmgf76mMUBSWP
qQIDAQAB
-----END PUBLIC KEY-----`;

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

        const encryptedPassword = crypto.publicEncrypt({
            key: publicKey,
            padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
            oaepHash: 'sha256',
        }, Buffer.from(password)).toString('base64');

        const stringifiedConfig = JSON.stringify({
            secrets: getAllSecrets(debouncedFormValues),
            config: debouncedFormValues,
        });
        const cipher = aesEncrypt({ password, input: Buffer.from(stringifiedConfig) });
        const encryptedConfig = cipher.toString('base64');

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
                            This allows you to directly build the firmware image on GitHub Actions.
                        </p>
                        <p>
                            Once you press "Start Build", we'll <strong>encrypt</strong> your configuration and send it over to GitHub to build the ESPHome image.
                            Once the build is complete, you can download a password protected Zip-file from the "Artifacts" section of your build.
                        </p>
                    </DialogContentText>
                </DialogContent>
                <DialogActions>
                    <Button onClick={closeModal} color="primary" disabled={isLoading}>
                        Cancel
                    </Button>
                    <Button onClick={handleBuild} color="primary" disabled={isLoading || password.length < 8 || identifier.trim().length === 0}>
                        {isLoading ? <CircularProgress size={24} /> : 'Start Build'}
                    </Button>
                </DialogActions>
            </Dialog>

            <Dialog open={isInstructionsModalOpen} onClose={handleCloseInstructionsModal}>
                <DialogTitle>Next Steps</DialogTitle>
                <DialogContent>
                    <DialogContentText>
                        Your build has been started now. Here's what you need to do next:
                        <ol>
                            <li>
                                Open the list of builds: <Link href="https://github.com/tomquist/esphome-b2500/actions"
                                                               target="_blank" rel="noopener">Build List</Link>
                            </li>
                            <li>
                                Locate your build: <strong>[B2500] {identifier}</strong> and click on it.
                            </li>
                            <li>Wait for the build to finish. If the build fails, please <Link
                                href={newIssueLink(debouncedFormValues)} target="_blank"
                                rel="noopener">open an issue</Link> with details of your configuration.
                            </li>
                            <li>Refresh the page, then locate the firmware artifact and download it:
                                <Box my={2}>
                                    <img src="/esphome-b2500/artifact.png" alt="Artifact" width="100%"/>
                                </Box>
                            </li>
                            <li>
                                Unzip the file using the entered password:
                                <strong onClick={handlePasswordCopy} style={{cursor: 'pointer', marginLeft: '5px'}}>
                                    {password} <ContentCopy />
                                </strong>
                            </li>
                            <li>
                                Open <Link href="https://web.esphome.io/" target="_blank" rel="noopener">ESPHome
                                Web</Link>, connect to your ESP32, click "Install" and select the unzipped .bin file.
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
