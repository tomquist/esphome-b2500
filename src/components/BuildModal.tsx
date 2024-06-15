import React from 'react';
import {
    Dialog,
    DialogActions,
    DialogContent,
    DialogContentText,
    DialogTitle,
    TextField,
    Button,
    InputAdornment,
    IconButton,
    CircularProgress,
} from '@mui/material';
import { ContentCopy } from '@mui/icons-material';
import axios from 'axios';
import crypto from 'crypto';
import { getAllSecrets, aesEncrypt } from '../utils';

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
    isModalOpen: boolean;
    closeModal: () => void;
    identifier: string;
    setIdentifier: (value: string) => void;
    password: string;
    setPassword: (value: string) => void;
    passwordError: string;
    setPasswordError: (value: string) => void;
    isLoading: boolean;
    setIsLoading: (value: boolean) => void;
    debouncedFormValues: any;
}

const BuildModal: React.FC<BuildModalProps> = ({
                                                   isModalOpen,
                                                   closeModal,
                                                   identifier,
                                                   setIdentifier,
                                                   password,
                                                   setPassword,
                                                   passwordError,
                                                   setPasswordError,
                                                   isLoading,
                                                   setIsLoading,
                                                   debouncedFormValues,
                                               }) => {
    const handleBuild = async () => {
        if (!debouncedFormValues) return;
        if (password.length < 8) {
            setPasswordError('Password must be at least 8 characters long.');
            return;
        }

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
            alert('Password copied to clipboard. Waiting for build to finish...');
            window.open('https://github.com/tomquist/esphome-b2500/actions/workflows/build-esphome.yml', '_blank');
        } catch (error) {
            console.error('Error triggering build:', error);
            alert('Failed to trigger build. Check the console for more details.');
        } finally {
            setIsLoading(false);
            closeModal();
        }
    };

    const handlePasswordCopy = () => {
        navigator.clipboard.writeText(password);
        alert('Password copied to clipboard');
    };

    return (
        <Dialog open={isModalOpen} onClose={closeModal} disableEscapeKeyDown={isLoading}>
            <DialogTitle>Enter Build Identifier</DialogTitle>
            <DialogContent>
                <DialogContentText>
                    This allows you to directly build the firmware image on GitHub Actions.
                    Once you submit, the build will start and create a password-encrypted zip file with the firmware.
                    Once the build is complete, you can download the zip file from the "Artifacts" section of your build.

                    Please enter a unique name for this build. This will help you identify your build process.
                </DialogContentText>
                <TextField
                    autoFocus
                    margin="dense"
                    label="Identifier"
                    type="text"
                    fullWidth
                    value={identifier}
                    onChange={(e) => setIdentifier(e.target.value)}
                    disabled={isLoading}
                />
                <DialogContentText>
                    Generated Password for Zipped Firmware:
                </DialogContentText>
                <TextField
                    margin="dense"
                    label="Password"
                    type="text"
                    fullWidth
                    value={password}
                    onChange={(e) => {
                        setPassword(e.target.value);
                        if (e.target.value.length >= 8) {
                            setPasswordError('');
                        } else {
                            setPasswordError('Password must be at least 8 characters long.');
                        }
                    }}
                    error={!!passwordError}
                    helperText={passwordError}
                    InputProps={{
                        endAdornment: (
                            <InputAdornment position="end">
                                <IconButton onClick={handlePasswordCopy}>
                                    <ContentCopy />
                                </IconButton>
                            </InputAdornment>
                        ),
                    }}
                    disabled={isLoading}
                />
            </DialogContent>
            <DialogActions>
                <Button onClick={closeModal} color="primary" disabled={isLoading}>
                    Cancel
                </Button>
                <Button onClick={handleBuild} color="primary" disabled={isLoading || password.length < 8 || identifier.trim().length === 0}>
                    {isLoading ? <CircularProgress size={24} /> : 'Submit'}
                </Button>
            </DialogActions>
        </Dialog>
    );
};

export default BuildModal;
