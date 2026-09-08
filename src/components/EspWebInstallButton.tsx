import React, { useEffect, useState } from 'react';
import { Alert, Button, CircularProgress } from '@mui/material';
import UsbIcon from '@mui/icons-material/Usb';

declare global {
  // eslint-disable-next-line @typescript-eslint/no-namespace
  namespace JSX {
    interface IntrinsicElements {
      'esp-web-install-button': React.DetailedHTMLProps<
        React.HTMLAttributes<HTMLElement>,
        HTMLElement
      > & { manifest?: string };
    }
  }
}

/**
 * Flashing from the browser needs the Web Serial API, which is only available
 * in Chromium based browsers and in secure contexts.
 */
export const isFlashingSupported = (): boolean =>
  typeof navigator !== 'undefined' &&
  'serial' in navigator &&
  typeof window !== 'undefined' &&
  window.isSecureContext;

interface EspWebInstallButtonProps {
  manifestUrl: string;
  label?: string;
}

/**
 * Loads esp-web-tools on demand and renders its install button with a MUI
 * button as the activation element.
 */
const EspWebInstallButton: React.FC<EspWebInstallButtonProps> = ({
  manifestUrl,
  label = 'Connect & Install',
}) => {
  const [isLoaded, setIsLoaded] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    import('esp-web-tools')
      .then(() => {
        if (!cancelled) {
          setIsLoaded(true);
        }
      })
      .catch((loadError) => {
        console.error('Failed to load the ESP web installer', loadError);
        if (!cancelled) {
          setError(
            'The installer could not be loaded. Please reload the page or use the manual instructions below.'
          );
        }
      });
    return () => {
      cancelled = true;
    };
  }, []);

  if (error) {
    return <Alert severity="error">{error}</Alert>;
  }

  if (!isLoaded) {
    return (
      <Button
        variant="contained"
        disabled
        startIcon={<CircularProgress size={16} />}
      >
        {label}
      </Button>
    );
  }

  return (
    <esp-web-install-button manifest={manifestUrl}>
      <Button slot="activate" variant="contained" startIcon={<UsbIcon />}>
        {label}
      </Button>
    </esp-web-install-button>
  );
};

export default EspWebInstallButton;
