// src/App.tsx
import React, { useState, useEffect, useCallback } from 'react';
import {
  Container,
  CssBaseline,
  ThemeProvider,
  createTheme,
  Box,
  Paper,
  Typography,
  Button,
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  TextField,
  InputAdornment,
  IconButton,
  CircularProgress,
} from '@mui/material';
import { FileCopy, Download, Build, ContentCopy } from '@mui/icons-material';
import ConfigForm from './components/ConfigForm';
import nunjucks from 'nunjucks';
import template from './template.jinja2';
import FileSaver from 'file-saver';
import { FormValues } from './types';
import { useDebounce } from './hooks/useDebounce';
import SyntaxHighlighterComponent from './components/SyntaxHighlighterComponent';
import axios from 'axios';
import crypto from 'crypto';
import {aesEncrypt} from "./crypto";

nunjucks.configure({ autoescape: false });

const theme = createTheme();

const publicKey = `-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvvRsA62G+HxFVteRvo9R
at3tt1hLzO1C//n0PgE0ljrc4Y5p+xfIrMe9lgo0Y0+/I5xtpOyzI6cCGiQfmpH8
v1/ZPwRJjG5Oezj910p7B90Z+bg2lS1H2BBPEq2CiDbUCtcd5RKZBiVv9Yjmwm/J
9DOGfiAB8lgmVReqe5fAc8IiZUnljp01LdCoxesp6HKSSEalet1pydj0joTR2xWy
80m58r69liFdLiZBzXYIlbpJRt91KSUyFEeRginm6mkrEjzu8bPKiZlXA3ZvbcSa
JSumPOJxYkcpse3uabXIEYRV3CL35R6CQN9YEYDYoGBrrQeAO5xgmgf76mMUBSWP
qQIDAQAB
-----END PUBLIC KEY-----`;

const defaultFormValues: FormValues = {
  name: 'b2500',
  friendly_name: 'B2500',
  mqtt: {
    topic: 'b2500',
    broker: 'mqttbroker.local',
    port: 1883,
    username: '',
    password: '',
    discovery: false,
  },
  wifi: {
    ssid: 'MyWifi',
    password: 'MyPassword',
  },
  board: 'esp32-c3-devkitm-1',
  enable_auto_restart: true,
  auto_restart: {
    restart_after_error_count: 8,
  },
  enable_cellquery: false,
  enable_cmd30: false,
  enable_esp_temperature: false,
  enable_powermeter: false,
  enable_experimental_commands: false,
  enable_hexdump: false,
  enable_set_wifi: false,
  set_wifi: {
    ssid: 'MyWifi',
    password: 'MyPassword',
  },
  enable_set_mqtt: false,
  powermeter: {
    tx_pin: 'GPIO6',
    rx_pin: 'GPIO7',
    baud_rate: 9600,
    stop_bits: 1,
  },
  enable_enforce_dod: false,
  enable_powerzero: false,
  powerzero: {
    grid_power_topic: 'tibber-esp/sensor/power/state',
    limit_cmd_topic: 'openDTU/XXXXXXXXXXXX/cmd/limit_persistent_relative',
    limit_state_topic: 'openDTU/XXXXXXXXXXXX/state/limit_relative',
  },
  enable_manual_ip: false,
  manual_ip: {
    ip: '192.168.1.100',
    gateway: '192.168.1.1',
    subnet: '255.255.255.0',
    dns: '192.168.1.1',
  },
  enable_web_server: true,
  web_server: {
    port: 80,
    ota: false,
    js_include: './v2/www.js',
  },
  enable_ota: true,
  ota: {
    password: 'my_ota_password',
    enable_unprotected_writes: false,
  },
  enable_fallback_hotspot: true,
  fallback_hotspot: {
    ssid: 'ESPHome-b2500'
  },
  storages: [
    { name: "B2500", version: 1, mac_address: "00:00:00:00:00:00" },
  ],
};

// Function to recursively merge two objects
const mergeDeep = (target: any, source: any) => {
  for (const key of Object.keys(source)) {
    if (source[key] instanceof Object) {
      if (!target[key]) {
        Object.assign(target, { [key]: {} });
      }
      mergeDeep(target[key], source[key]);
    } else {
      if (target[key] === undefined) {
        Object.assign(target, { [key]: source[key] });
      }
    }
  }
  return target;
};

function getAllSecrets(debouncedFormValues: FormValues) {
  const secrets = [];
  if (debouncedFormValues.mqtt.password) {
    secrets.push(debouncedFormValues.mqtt.password);
  }
  if (debouncedFormValues.wifi.password) {
    secrets.push(debouncedFormValues.wifi.password);
  }
  if (debouncedFormValues.set_wifi.password) {
    secrets.push(debouncedFormValues.set_wifi.password);
  }
  if (debouncedFormValues.ota.password) {
    secrets.push(debouncedFormValues.ota.password);
  }
  return secrets;
}


const generatePassword = () => {
  return Math.random().toString(36).slice(-8);
};

const App: React.FC = () => {
  const [config, setConfig] = useState<string>('');
  const [formValues, setFormValues] = useState<FormValues>();
  const [isModalOpen, setIsModalOpen] = useState<boolean>(false);
  const [identifier, setIdentifier] = useState<string>('');
  const [password, setPassword] = useState<string>('');
  const [passwordError, setPasswordError] = useState<string>('');
  const [isLoading, setIsLoading] = useState<boolean>(false);

  const debouncedFormValues = useDebounce(formValues, 200);

  useEffect(() => {
    const storedFormValues = localStorage.getItem('formValues');
    if (storedFormValues != null) {
      const loadedValues = JSON.parse(storedFormValues);
      const mergedValues = mergeDeep(loadedValues, defaultFormValues);
      setFormValues(mergedValues);
    } else {
      setFormValues(defaultFormValues);
    }
  }, []);

  useEffect(() => {
    if (!debouncedFormValues) {
      return;
    }
    const renderedConfig = nunjucks.renderString(template, debouncedFormValues);
    setConfig(renderedConfig);
    localStorage.setItem('formValues', JSON.stringify(debouncedFormValues));
  }, [debouncedFormValues]);

  const handleFormChange = useCallback((newFormValues: FormValues) => {
    setFormValues(newFormValues);
  }, []);

  const handleDownload = () => {
    const blob = new Blob([config], { type: 'text/yaml;charset=utf-8' });
    FileSaver.saveAs(blob, 'config.yaml');
  };

  const handleCopy = () => {
    navigator.clipboard.writeText(config);
  };

  const handleBuild = async () => {
    if (debouncedFormValues == null) {
      return;
    }
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
    })
    const cipher = aesEncrypt({password, input: Buffer.from(stringifiedConfig)});
    const encryptedConfig = cipher.toString('base64');

    try {
      await axios.post(
        'https://publicactiontrigger.azurewebsites.net/api/dispatches/tomquist/esphome-b2500',
        {
          event_type: 'build_esphome',
          client_payload: {
            config: encryptedConfig,
            identifier,
            password: encryptedPassword
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
      setIsModalOpen(false);
    }
  };

  const openModal = () => {
    setPassword(generatePassword());
    setPasswordError('');
    setIsModalOpen(true);
  };

  const closeModal = () => {
    setIsModalOpen(false);
  };

  const handleIdentifierChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    setIdentifier(event.target.value);
  };

  const handlePasswordChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    setPassword(event.target.value);
    if (event.target.value.length >= 8) {
      setPasswordError('');
    } else {
      setPasswordError('Password must be at least 8 characters long.');
    }
  };

  const handlePasswordCopy = () => {
    navigator.clipboard.writeText(password);
    alert('Password copied to clipboard');
  };

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <Container>
        <Typography variant="h4" align="center" gutterBottom>
          ESPHome B2500 Config Generator
        </Typography>
        <Box mt={4}>
          {formValues && <ConfigForm formValues={formValues} onFormChange={handleFormChange} />}
        </Box>
        {config && (
          <Box mt={4}>
            <Paper elevation={3} sx={{ padding: 2, position: 'relative' }}>
              <Box display="flex" justifyContent="flex-end" mb={2}>
                <Button
                  variant="contained"
                  color="primary"
                  startIcon={<FileCopy />}
                  onClick={handleCopy}
                  sx={{ marginRight: 2 }}
                >
                  Copy to Clipboard
                </Button>
                <Button
                  variant="contained"
                  color="primary"
                  startIcon={<Download />}
                  onClick={handleDownload}
                  sx={{ marginRight: 2 }}
                >
                  Download YAML
                </Button>
                <Button
                  variant="contained"
                  color="secondary"
                  startIcon={<Build />}
                  onClick={openModal}
                >
                  Build Image
                </Button>
              </Box>
              <Typography variant="h6" gutterBottom>
                Generated YAML Config
              </Typography>
              <SyntaxHighlighterComponent config={config} />
            </Paper>
          </Box>
        )}
      </Container>

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
            onChange={handleIdentifierChange}
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
            onChange={handlePasswordChange}
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
    </ThemeProvider>
  );
};

export default App;

