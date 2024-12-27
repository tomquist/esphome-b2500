import React, { useState, useEffect } from 'react';
import {
  Container,
  CssBaseline,
  ThemeProvider,
  createTheme,
  Box,
  Paper,
  Typography,
  Button,
  Tooltip,
} from '@mui/material';
import { FileCopy, Download, Build } from '@mui/icons-material';
import nunjucks from 'nunjucks';
import template from './template.jinja2';
import templateV2 from './template_v2.jinja2';
import templateV2Minimal from './template_v2_minimal.jinja2';
import templateMqttRelay from './template_relay.jinja2';
import FileSaver from 'file-saver';
import { FormValues } from './types';
import { useDebounce } from './hooks/useDebounce';
import SyntaxHighlighterComponent from './components/SyntaxHighlighterComponent';
import ConfigForm from './components/ConfigForm';
import BuildModal from './components/BuildModal';
import {
  defaultFormValues,
  mergeDeep,
  newIssueLink,
  validateConfig,
} from './utils';
import GetAppOutlinedIcon from '@mui/icons-material/GetAppOutlined';
import PublishOutlinedIcon from '@mui/icons-material/PublishOutlined';
import { useMediaQuery } from '@mui/material';

nunjucks.configure({ autoescape: false });

const theme = createTheme();
const darkTheme = createTheme({
  palette: {
    mode: 'dark',
    primary: {
      main: '#90caf9', // customize primary color for dark mode if needed
    },
  },
});

const App: React.FC = () => {
  const prefersDarkMode = useMediaQuery('(prefers-color-scheme: dark)');
  const themeToUse = prefersDarkMode ? darkTheme : theme;
  const [config, setConfig] = useState<string>('');
  const [formValues, setFormValues] = useState<FormValues>();
  const [isModalOpen, setIsModalOpen] = useState<boolean>(false);

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
    if (!debouncedFormValues) return;
    let templateToUse;
    switch (debouncedFormValues?.template_version ?? 'v1') {
      case 'v1':
        templateToUse = template;
        break;
      case 'v2':
        templateToUse = templateV2;
        break;
      case 'v2-minimal':
        templateToUse = templateV2Minimal;
        break;
      case 'mqtt-relay':
        templateToUse = templateMqttRelay;
        break;
    }
    const renderedConfig = nunjucks.renderString(
      templateToUse,
      debouncedFormValues
    );
    setConfig(renderedConfig);
    localStorage.setItem('formValues', JSON.stringify(debouncedFormValues));
  }, [debouncedFormValues]);

  const handleFormChange = React.useCallback((newFormValues: FormValues) => {
    setFormValues(newFormValues);
  }, []);

  const handleDownload = React.useCallback(() => {
    const blob = new Blob([config], { type: 'text/yaml;charset=utf-8' });
    FileSaver.saveAs(blob, 'config.yaml');
  }, [config]);

  const handleCopy = React.useCallback(() => {
    navigator.clipboard.writeText(config);
  }, [config]);

  const openModal = React.useCallback(() => {
    setIsModalOpen(true);
  }, [setIsModalOpen]);

  const closeModal = React.useCallback(() => {
    setIsModalOpen(false);
  }, [setIsModalOpen]);

  const errors = React.useMemo(
    () => (debouncedFormValues ? validateConfig(debouncedFormValues) : []),
    [debouncedFormValues]
  );

  const handleExport = () => {
    const json = JSON.stringify(formValues, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'esphome-b2500.config.json';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  const handleImport = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (event) => {
        try {
          const importedData = JSON.parse(event.target?.result as string);
          const mergedValues = mergeDeep(importedData, defaultFormValues);
          setFormValues(mergedValues);
        } catch (error) {
          console.error('Error parsing JSON file', error);
        }
      };
      reader.readAsText(file);

      // Reset file input value to allow re-selection of the same file
      e.target.value = ''; // Reset the value to trigger onChange next time
    }
  };

  return (
    <ThemeProvider theme={themeToUse}>
      <CssBaseline />
      <Container>
        <Typography variant="h4" align="center" gutterBottom>
          ESPHome B2500 Config Generator
        </Typography>
        <Paper elevation={3} sx={{ padding: 2 }}>
          <Typography variant="body1" paragraph>
            Use this form to generate an ESPHome config to connect your ESP32 to
            a B2500 storage. If you encounter any issues, click the button below
            to report a problem.
          </Typography>
          <Button
            variant="contained"
            color="primary"
            href={formValues ? newIssueLink({ config: formValues }) : ''}
            target="_blank"
            rel="noopener"
            sx={{ mb: 2 }}
          >
            Report an Issue
          </Button>
        </Paper>
        <Box mt={4}>
          {formValues && (
            <ConfigForm
              formValues={formValues}
              onFormChange={handleFormChange}
            />
          )}
        </Box>
        {config && (
          <Box mt={4}>
            <Paper elevation={3} sx={{ padding: 2, position: 'relative' }}>
              <Box display="flex" justifyContent="flex-end" mb={2}>
                <Button
                  variant="contained"
                  color="success"
                  onClick={handleExport}
                  sx={{ marginRight: 2 }}
                  startIcon={<GetAppOutlinedIcon />}
                >
                  Export as JSON
                </Button>
                <label htmlFor="import-json">
                  <input
                    id="import-json"
                    type="file"
                    accept=".json"
                    onChange={handleImport}
                    style={{ display: 'none' }}
                  />
                  <Button
                    variant="contained"
                    color="success"
                    component="span"
                    sx={{ marginRight: 2 }}
                    startIcon={<PublishOutlinedIcon />}
                  >
                    Import JSON
                  </Button>
                </label>
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
                <Tooltip
                  title={
                    errors.length > 0
                      ? `Please fix the errors before building the image: ${errors.join(
                          ', '
                        )}`
                      : undefined
                  }
                >
                  <Button
                    variant="contained"
                    color="secondary"
                    startIcon={<Build />}
                    onClick={openModal}
                    disabled={errors.length > 0}
                  >
                    Build Image
                  </Button>
                </Tooltip>
              </Box>
              <Typography variant="h6" gutterBottom>
                Generated YAML Config
              </Typography>
              <SyntaxHighlighterComponent config={config} />
            </Paper>
          </Box>
        )}
      </Container>
      {isModalOpen && (
        <BuildModal
          closeModal={closeModal}
          debouncedFormValues={debouncedFormValues}
        />
      )}
    </ThemeProvider>
  );
};

export default App;
