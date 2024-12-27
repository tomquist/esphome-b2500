// src/components/ConfigForm.tsx
import React, { useCallback } from 'react';
import {
  Paper,
  Typography,
  Divider,
  TextField,
  Link,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  SelectChangeEvent,
  FormHelperText,
} from '@mui/material';
import {
  EspTemperatureSettings,
  FormValues,
  Storage,
  validPlatformVariants,
} from '../types';
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
  const handleSelectChange = useCallback(
    <E extends string>(e: SelectChangeEvent<E>) => {
      const { name, value } = e.target;
      onFormChange({
        ...formValues,
        [name]: value,
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
  const handleEspTemperatureChange = useCallback(
    (temperatureSettings: EspTemperatureSettings) => {
      onFormChange({
        ...formValues,
        esp_temperature: temperatureSettings,
      });
    },
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

  let nameInvalid =
    formValues.name.length > 31 || !/^[a-z0-9-]+$/.test(formValues.name);
  let friendlyNameInvalid =
    formValues.friendly_name.length > 63 ||
    formValues.friendly_name.trim() === '';
  return (
    <Paper elevation={3} sx={{ padding: 2 }}>
      <Typography variant="h6">General Settings</Typography>
      <FormControl fullWidth margin="normal">
        <InputLabel id="variant-label">Configuration Version</InputLabel>
        <Select
          labelId="template-version-label"
          name="template_version"
          value={formValues.template_version}
          onChange={handleSelectChange}
        >
          <MenuItem value={'v1'}>Legacy</MenuItem>
          <MenuItem value={'v2'}>Native ESPHome component</MenuItem>
          <MenuItem value={'v2-minimal'}>
            Minimal native ESPHome component
          </MenuItem>
          <MenuItem value={'mqtt-relay'}>MQTT Relay</MenuItem>
        </Select>
        <FormHelperText>
          You can choose between two different configuration versions:
          <ul>
            <li>
              Legacy - Old config based on{' '}
              <a href="https://github.com/noone2k/hm2500pub/tree/master">
                noone2k's ESPHome config
              </a>
            </li>
            <li>
              Native ESPHome component - A new config that uses a native ESPHome
              component. Slightly less resource-intensive and more stable.
            </li>
            <li>
              Minimal native ESPHome component - A minimal version of the native
              ESPHome component. Exposes storage data through a smaller number
              of MQTT topics without using ESPHome sensors. Use this for
              low-power devices or if you manually integrate the storage into
              your home automation system using MQTT.
            </li>
            <li>
              MQTT Relay - A minimal config that relays MQTT data from the
              storage to the Hame MQTT broker. It doesn't expose any sensors or
              switches and doesn't connect to the storage via Bluetooth. With
              this you can configure the storage to send data to your local MQTT
              broker while still being able to use the Power Zero/Marstek app to
              control the storage. Note that an image for this can only be built
              via this tool. You won't be able to build it using the ESPHome
              unless you know the Hame certificate and key.
            </li>
          </ul>
        </FormHelperText>
      </FormControl>
      <TextField
        label="Name"
        name="name"
        value={formValues.name}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
        helperText="This is the name of the node. It should always be unique in your ESPHome network. May only contain lowercase characters, digits and hyphens, and can be at most 31 characters long."
        error={nameInvalid}
        required
      />
      <TextField
        label="Friendly Name"
        name="friendly_name"
        value={formValues.friendly_name}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
        required
        helperText="This is the name sent to the frontend. It is used by Home Assistant as the integration name, device name, and is automatically prefixed to entities where necessary."
        error={friendlyNameInvalid}
      />
      <TextField
        label="Board"
        name="board"
        value={formValues.board}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
        required
        error={formValues.board.trim() === ''}
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

      <FormControl fullWidth margin="normal">
        <InputLabel id="variant-label">Variant</InputLabel>
        <Select
          labelId="variant-label"
          name="variant"
          value={formValues.variant}
          onChange={handleSelectChange}
        >
          {validPlatformVariants.map((variant) => (
            <MenuItem key={variant} value={variant}>
              {variant}
            </MenuItem>
          ))}
        </Select>
        <FormHelperText>
          The variant of the ESP32 that is used on this board. If unsure, leave
          this at "auto" to automatically detect the variant from the board.
        </FormHelperText>
      </FormControl>

      <Divider sx={{ my: 2 }} />

      <MqttSection
        mqtt={formValues.mqtt}
        enable_set_mqtt={formValues.enable_set_mqtt}
        handleMQTTInputChange={handleMQTTInputChange}
      />
      <WifiSection
        wifi={formValues.wifi}
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
        handleEspTemperatureChange={handleEspTemperatureChange}
      />

      <Divider sx={{ my: 2 }} />

      <StorageForm
        templateVersion={formValues.template_version}
        storages={formValues.storages}
        onChange={handleStorageChange}
        maxStorages={3}
        minStorages={1}
      />
    </Paper>
  );
};

export default ConfigForm;
