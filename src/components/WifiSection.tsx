import React from 'react';
import {
  Accordion,
  AccordionSummary,
  AccordionDetails,
  TextField,
  Typography,
} from '@mui/material';
import { ExpandMore } from '@mui/icons-material';
import { FormValues } from '../types';

interface WifiSectionProps {
  wifi: FormValues['wifi'];
  handleWifiInputChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
}

const WifiSection: React.FC<WifiSectionProps> = ({
  wifi,
  handleWifiInputChange,
}) => {
  let ssidInvalid = wifi.ssid.trim() === '';
  let passwordInvalid = wifi.password.trim() === '';
  return (
    <Accordion>
      <AccordionSummary expandIcon={<ExpandMore />}>
        <Typography variant="h6">Wifi</Typography>
      </AccordionSummary>
      <AccordionDetails>
        <TextField
          label="SSID"
          name="ssid"
          value={wifi.ssid}
          onChange={handleWifiInputChange}
          fullWidth
          margin="normal"
          required
          error={ssidInvalid}
          helperText={ssidInvalid ? 'SSID is required' : ''}
        />
        <TextField
          label="Password"
          name="password"
          type="password"
          value={wifi.password}
          onChange={handleWifiInputChange}
          fullWidth
          margin="normal"
          required
          error={passwordInvalid}
          helperText={passwordInvalid ? 'Password is required' : ''}
        />
      </AccordionDetails>
    </Accordion>
  );
};

export default WifiSection;
