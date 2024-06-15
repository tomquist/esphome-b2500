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
  formValues: FormValues;
  handleWifiInputChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
}

const WifiSection: React.FC<WifiSectionProps> = ({
  formValues,
  handleWifiInputChange,
}) => (
  <Accordion>
    <AccordionSummary expandIcon={<ExpandMore />}>
      <Typography variant="h6">Wifi</Typography>
    </AccordionSummary>
    <AccordionDetails>
      <TextField
        label="SSID"
        name="ssid"
        value={formValues.wifi.ssid}
        onChange={handleWifiInputChange}
        fullWidth
        margin="normal"
      />
      <TextField
        label="Password"
        name="password"
        type="password"
        value={formValues.wifi.password}
        onChange={handleWifiInputChange}
        fullWidth
        margin="normal"
      />
    </AccordionDetails>
  </Accordion>
);

export default WifiSection;
