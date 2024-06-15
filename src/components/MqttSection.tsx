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
  mqtt: FormValues['mqtt'];
  enable_set_mqtt: boolean;
  handleMQTTInputChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
}

const MqttSection: React.FC<MqttSectionProps> = ({
  mqtt,
  enable_set_mqtt,
  handleMQTTInputChange,
}) => {
  let topicInvalid = mqtt.topic.length === 0;
  let brokerInvalid = mqtt.broker.trim() === '';
  let portInvalid = !mqtt.port || mqtt.port < 1 || mqtt.port > 65535;
  return (
    <Accordion>
      <AccordionSummary expandIcon={<ExpandMore />}>
        <Typography variant="h6">MQTT</Typography>
      </AccordionSummary>
      <AccordionDetails>
        <TextField
          label="Topic"
          name="topic"
          value={mqtt.topic}
          onChange={handleMQTTInputChange}
          fullWidth
          margin="normal"
          required
          error={topicInvalid}
          helperText={topicInvalid ? 'Topic is required' : ''}
        />
        <TextField
          label="Broker"
          name="broker"
          value={mqtt.broker}
          onChange={handleMQTTInputChange}
          fullWidth
          margin="normal"
          required
          error={brokerInvalid}
          helperText={brokerInvalid ? 'Broker is required' : ''}
        />
        <TextField
          label="Port"
          name="port"
          type="number"
          inputProps={{ min: 1, max: 65535 }}
          value={mqtt.port}
          onChange={handleMQTTInputChange}
          fullWidth
          margin="normal"
          required
          error={portInvalid}
          helperText={portInvalid ? 'Port is invalid' : ''}
        />
        <TextField
          label="Username"
          name="username"
          value={mqtt.username}
          onChange={handleMQTTInputChange}
          fullWidth
          margin="normal"
          required={enable_set_mqtt || mqtt.password.length > 0}
        />
        <TextField
          label="Password"
          name="password"
          type="password"
          value={mqtt.password}
          onChange={handleMQTTInputChange}
          fullWidth
          margin="normal"
          required={enable_set_mqtt}
        />
        <BooleanField
          value={mqtt}
          onChange={handleMQTTInputChange}
          prop="discovery"
          label="Discovery"
        />
      </AccordionDetails>
    </Accordion>
  );
};

export default MqttSection;
