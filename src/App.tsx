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

nunjucks.configure({ autoescape: false });

const theme = createTheme();

const App: React.FC = () => {
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
    const renderedConfig = nunjucks.renderString(template, debouncedFormValues);
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

  return (
    <ThemeProvider theme={theme}>
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
