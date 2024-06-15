import React from 'react';
import {
  Accordion,
  AccordionSummary,
  AccordionDetails,
  TextField,
  Typography,
} from '@mui/material';
import { ExpandMore } from '@mui/icons-material';
import BooleanField from './BooleanField';
import { FormValues } from '../types';

interface MqttSectionProps {
  formValues: FormValues;
  handleMQTTInputChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
}

const MqttSection: React.FC<MqttSectionProps> = ({
  formValues,
  handleMQTTInputChange,
}) => (
  <Accordion>
    <AccordionSummary expandIcon={<ExpandMore />}>
      <Typography variant="h6">MQTT</Typography>
    </AccordionSummary>
    <AccordionDetails>
      <TextField
        label="Topic"
        name="topic"
        value={formValues.mqtt.topic}
        onChange={handleMQTTInputChange}
        fullWidth
        margin="normal"
        error={formValues.mqtt.topic.length === 0}
      />
      <TextField
        label="Broker"
        name="broker"
        value={formValues.mqtt.broker}
        onChange={handleMQTTInputChange}
        fullWidth
        margin="normal"
      />
      <TextField
        label="Port"
        name="port"
        type="number"
        inputProps={{ min: 1, max: 65535 }}
        value={formValues.mqtt.port}
        onChange={handleMQTTInputChange}
        fullWidth
        margin="normal"
        error={formValues.mqtt.port < 1 || formValues.mqtt.port > 65535}
      />
      <TextField
        label="Username"
        name="username"
        value={formValues.mqtt.username}
        onChange={handleMQTTInputChange}
        fullWidth
        margin="normal"
      />
      <TextField
        label="Password"
        name="password"
        type="password"
        value={formValues.mqtt.password}
        onChange={handleMQTTInputChange}
        fullWidth
        margin="normal"
      />
      <BooleanField
        value={formValues.mqtt}
        onChange={handleMQTTInputChange}
        prop="discovery"
        label="Discovery"
      />
    </AccordionDetails>
  </Accordion>
);

export default MqttSection;
