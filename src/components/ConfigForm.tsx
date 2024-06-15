// src/components/ConfigForm.tsx
import React, { useCallback } from 'react';
import { Paper, Typography, Divider, TextField, Link } from '@mui/material';
import { FormValues, Storage } from '../types';
import { makeHandleInputChange } from '../utils/formUtils';
import MqttSection from './MqttSection';
import WifiSection from './WifiSection';
import AdvancedSection from './AdvancedSection';
import { StorageForm } from './StorageForm';

interface ConfigFormProps {
  formValues: FormValues;
  onFormChange: (data: FormValues) => void;
}

const ConfigForm: React.FC<ConfigFormProps> = ({
  formValues,
  onFormChange,
}) => {
  const handleInputChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const { name, value, type, checked } = e.target;
      onFormChange({
        ...formValues,
        [name]: type === 'checkbox' ? checked : value,
      });
    },
    [formValues, onFormChange]
  );

  /* eslint-disable react-hooks/exhaustive-deps */
  const handleMQTTInputChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('mqtt'),
    [formValues, onFormChange]
  );
  const handleWifiInputChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('wifi'),
    [formValues, onFormChange]
  );
  const handleAutoRestartChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('auto_restart'),
    [formValues, onFormChange]
  );
  const handleSetWifiChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('set_wifi'),
    [formValues, onFormChange]
  );
  const handleWebServerChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('web_server'),
    [formValues, onFormChange]
  );
  const handleOtaChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('ota'),
    [formValues, onFormChange]
  );
  const handleFallbackHotspotChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('fallback_hotspot'),
    [formValues, onFormChange]
  );
  const handleManualIPChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('manual_ip'),
    [formValues, onFormChange]
  );
  const handlePowerzeroChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('powerzero'),
    [formValues, onFormChange]
  );
  const handlePowermeterChange = useCallback(
    makeHandleInputChange(formValues, onFormChange)('powermeter'),
    [formValues, onFormChange]
  );
  /* eslint-enable react-hooks/exhaustive-deps */

  const handleStorageChange = useCallback(
    (storages: Storage[]) => {
      onFormChange({
        ...formValues,
        storages,
      });
    },
    [formValues, onFormChange]
  );

  return (
    <Paper elevation={3} sx={{ padding: 2 }}>
      <Typography variant="h6">General Settings</Typography>
      <TextField
        label="Name"
        name="name"
        value={formValues.name}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
        helperText="This is the name of the node. It should always be unique in your ESPHome network. May only contain lowercase characters, digits and hyphens, and can be at most 31 characters long."
        error={
          formValues.name.length > 31 || !/^[a-z0-9-]+$/.test(formValues.name)
        }
      />
      <TextField
        label="Friendly Name"
        name="friendly_name"
        value={formValues.friendly_name}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
        helperText="This is the name sent to the frontend. It is used by Home Assistant as the integration name, device name, and is automatically prefixed to entities where necessary."
      />
      <TextField
        label="Board"
        name="board"
        value={formValues.board}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
        helperText={
          <span>
            The PlatformIO board ID. Choose the appropriate board from this{' '}
            <Link
              href="https://registry.platformio.org/packages/platforms/platformio/espressif32/boards"
              target="_blank"
              rel="noopener"
            >
              list
            </Link>
            . If unsure, choose a generic board such as <code>esp32dev</code>.
          </span>
        }
      />

      <Divider sx={{ my: 2 }} />

      <MqttSection
        formValues={formValues}
        handleMQTTInputChange={handleMQTTInputChange}
      />
      <WifiSection
        formValues={formValues}
        handleWifiInputChange={handleWifiInputChange}
      />
      <AdvancedSection
        formValues={formValues}
        handleInputChange={handleInputChange}
        handleAutoRestartChange={handleAutoRestartChange}
        handleSetWifiChange={handleSetWifiChange}
        handleWebServerChange={handleWebServerChange}
        handleOtaChange={handleOtaChange}
        handleFallbackHotspotChange={handleFallbackHotspotChange}
        handleManualIPChange={handleManualIPChange}
        handlePowerzeroChange={handlePowerzeroChange}
        handlePowermeterChange={handlePowermeterChange}
      />

      <Divider sx={{ my: 2 }} />

      <StorageForm
        storages={formValues.storages}
        onChange={handleStorageChange}
        maxStorages={3}
        minStorages={1}
      />
    </Paper>
  );
};

export default ConfigForm;
