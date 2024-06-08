// src/components/ConfigForm.tsx
import React, { useCallback } from 'react';
import {
  Box,
  TextField,
  Checkbox,
  FormControlLabel,
  Typography,
  Paper,
  Divider,
  Grid,
  Accordion,
  AccordionSummary,
  AccordionDetails,
} from '@mui/material';
import { ExpandMore } from '@mui/icons-material';
import { StorageForm } from './StorageForm';
import { FormValues, Storage } from '../types';

interface ConfigFormProps {
  formValues: FormValues;
  onFormChange: (data: FormValues) => void;
}

type KeysTyped<V, T> = keyof {
  [K in keyof V as V[K] extends T ? K : never]: V[K];
};

type BooleanFieldProps<T extends object> = {
  value: T,
  prop: KeysTyped<T, boolean> & string;
  onChange: (e: React.ChangeEvent<HTMLInputElement>) => void,
  label: string
};

const BooleanField = <T extends object>(props: BooleanFieldProps<T>) => {
  const {
    value,
    prop,
    onChange,
    label
  } = props;
  return (
    <FormControlLabel
      control={
        <Checkbox
          checked={value[prop] as boolean}
          onChange={onChange}
          name={prop}
        />
      }
      label={label}
    />
  );
}

const ConfigForm: React.FC<ConfigFormProps> = ({ formValues, onFormChange }) => {
  const handleInputChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value, type, checked } = e.target;
    onFormChange({
      ...formValues,
      [name]: type === 'checkbox' ? checked : value,
    });
  }, [formValues, onFormChange]);

  const makeHandleInputChange = (name: KeysTyped<FormValues, object>) => {
    return (e: React.ChangeEvent<HTMLInputElement>) => {
      const { value, type, checked, name: inputName } = e.target;
      onFormChange({
        ...formValues,
        [name]: {
          ...formValues[name] as any,
          [inputName]: type === 'checkbox' ? checked : value,
        }
      });
    };
  };
  /* eslint-disable react-hooks/exhaustive-deps */
  const handleAutoRestartChange = useCallback(makeHandleInputChange('auto_restart'), [formValues, onFormChange]);
  const handleMQTTInputChange = useCallback(makeHandleInputChange('mqtt'), [formValues, onFormChange]);
  const handleWifiInputChange = useCallback(makeHandleInputChange('wifi'), [formValues, onFormChange]);
  const handleSetWifiChange = useCallback(makeHandleInputChange('set_wifi'), [formValues, onFormChange]);
  const handleWebServerChange = useCallback(makeHandleInputChange('web_server'), [formValues, onFormChange]);
  const handleOtaChange = useCallback(makeHandleInputChange('ota'), [formValues, onFormChange]);
  const handleFallbackHotspotChange = useCallback(makeHandleInputChange('fallback_hotspot'), [formValues, onFormChange]);
  const handleManualIPChange = useCallback(makeHandleInputChange('manual_ip'), [formValues, onFormChange]);
  const handlePowerzeroChange = useCallback(makeHandleInputChange('powerzero'), [formValues, onFormChange]);
  const handlePowermeterChange = useCallback(makeHandleInputChange('powermeter'), [formValues, onFormChange]);
  /* eslint-enable react-hooks/exhaustive-deps */

  const handleStorageChange = useCallback((storages: Storage[]) => {
    onFormChange({
      ...formValues,
      storages,
    });
  }, [formValues, onFormChange]);

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
      />
      <TextField
        label="Friendly Name"
        name="friendly_name"
        value={formValues.friendly_name}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
      />
      <TextField
        label="Board"
        name="board"
        value={formValues.board}
        onChange={handleInputChange}
        fullWidth
        margin="normal"
      />

      <Divider sx={{ my: 2 }} />

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

      <Accordion>
        <AccordionSummary expandIcon={<ExpandMore />}>
          <Typography variant="h6">Advanced</Typography>
        </AccordionSummary>
        <AccordionDetails>
          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_auto_restart"
            label="Enable Auto Restart"
          />
          {formValues.enable_auto_restart && (
            <TextField
              label="Restart after error count"
              name="restart_after_error_count"
              type="number"
              value={formValues.auto_restart.restart_after_error_count}
              onChange={handleAutoRestartChange}
              fullWidth
              margin="normal"
            />
          )}

          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_hexdump"
            label="Enable Hexdump"
          />
          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_cellquery"
            label="Enable Cellquery"
          />
          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_experimental_commands"
            label="Enable Experimental Commands"
          />
          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_set_mqtt"
            label="Enable Set MQTT"
          />
          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop={"enable_set_wifi"}
            label={"Enable Set Wifi"}
          />
          {formValues.enable_set_wifi && (
            <Box mb={2} border={1} borderRadius={5} padding={2}>
              <TextField
                label="SSID"
                name="ssid"
                value={formValues.set_wifi.ssid}
                onChange={handleSetWifiChange}
                fullWidth
                margin="normal"
              />
              <TextField
                label="Password"
                name="password"
                type="password"
                value={formValues.set_wifi.password}
                onChange={handleSetWifiChange}
                fullWidth
                margin="normal"
              />
            </Box>
          )}

          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_enforce_dod"
            label="Enable Enforce DoD"
          />
          <BooleanField value={formValues}
            onChange={handleInputChange}
            prop="enable_web_server" label="Enable Web Server" />
          {formValues.enable_web_server && (
            <Box mb={2} border={1} borderRadius={5} padding={2}>
              <Grid container spacing={2}>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Port"
                    name="port"
                    type="number"
                    inputProps={{ min: 1, max: 65535 }}
                    value={formValues.web_server.port}
                    onChange={handleWebServerChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <FormControlLabel
                    control={
                      <Checkbox
                        checked={formValues.web_server.ota}
                        onChange={handleWebServerChange}
                        name="ota"
                      />
                    }
                    label="OTA"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="JS Include"
                    name="js_include"
                    value={formValues.web_server.js_include}
                    onChange={handleWebServerChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
              </Grid>
            </Box>
          )}

          <BooleanField value={formValues}
            onChange={handleInputChange}
            prop="enable_ota" label="Enable OTA" />
          {formValues.enable_ota && (
            <Box mb={2} border={1} borderRadius={5} padding={2}>
              <TextField
                label="Password"
                name="password"
                type="password"
                value={formValues.ota.password}
                onChange={handleInputChange}
                fullWidth
                margin="normal"
              />
              <BooleanField
                value={formValues.ota}
                onChange={handleOtaChange}
                prop="enable_unprotected_writes"
                label="Enable Unprotected Writes"
              />
            </Box>
          )}

          <BooleanField value={formValues}
            onChange={handleInputChange}
            prop="enable_fallback_hotspot" label="Enable Fallback Hotspot" />
          {formValues.enable_fallback_hotspot && (
            <TextField
              label="Fallback Hotspot SSID"
              name="ssid"
              value={formValues.fallback_hotspot.ssid}
              onChange={handleFallbackHotspotChange}
              fullWidth
              margin="normal"
            />
          )}
          <BooleanField value={formValues}
            onChange={handleInputChange}
            prop="enable_cmd13"
            label="Enable CMD13"
          />
          <BooleanField value={formValues}
            onChange={handleInputChange}
            prop="enable_cmd30" label="Enable CMD30" />
          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_esp_temperature"
            label="Enable ESP temperature"
          />
          <BooleanField value={formValues}
            onChange={handleInputChange}
            prop="enable_powermeter"
            label="Enable Powermeter"
          />
          {formValues.enable_powermeter && (
            <Box mb={2} border={1} borderRadius={5} padding={2}>
              <Grid container spacing={2}>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="TX Pin"
                    name="tx_pin"
                    value={formValues.powermeter.tx_pin}
                    onChange={handlePowermeterChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="RX Pin"
                    name="rx_pin"
                    value={formValues.powermeter.rx_pin}
                    onChange={handlePowermeterChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Baud Rate"
                    name="baud_rate"
                    type="number"
                    value={formValues.powermeter.baud_rate}
                    onChange={handlePowermeterChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Stop Bits"
                    name="stop_bits"
                    type="number"
                    inputProps={{ min: 1, max: 2 }}
                    value={formValues.powermeter.stop_bits}
                    onChange={handlePowermeterChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
              </Grid>
            </Box>
          )}

          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_manual_ip"
            label="Enable Manual IP"
          />
          {formValues.enable_manual_ip && (
            <Box mb={2} border={1} borderRadius={5} padding={2}>
              <Grid container spacing={2}>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="IP Address"
                    name="ip"
                    value={formValues.manual_ip.ip}
                    onChange={handleManualIPChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Gateway"
                    name="gateway"
                    value={formValues.manual_ip.gateway}
                    onChange={handleManualIPChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Subnet"
                    name="subnet"
                    value={formValues.manual_ip.subnet}
                    onChange={handleManualIPChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="DNS"
                    name="dns"
                    value={formValues.manual_ip.dns}
                    onChange={handleManualIPChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
              </Grid>
            </Box>
          )}

          <BooleanField
            value={formValues}
            onChange={handleInputChange}
            prop="enable_powerzero"
            label="Enable Powerzero"
          />
          {formValues.enable_powerzero && (
            <Box mb={2} border={1} borderRadius={5} padding={2}>
              <Grid container spacing={2}>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Grid Power Topic"
                    name="grid_power_topic"
                    value={formValues.powerzero.grid_power_topic}
                    onChange={handlePowerzeroChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Limit CMD Topic"
                    name="limit_cmd_topic"
                    value={formValues.powerzero.limit_cmd_topic}
                    onChange={handlePowerzeroChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Limit State Topic"
                    name="limit_state_topic"
                    value={formValues.powerzero.limit_state_topic}
                    onChange={handlePowerzeroChange}
                    fullWidth
                    margin="normal"
                  />
                </Grid>
              </Grid>
            </Box>
          )}
        </AccordionDetails>
      </Accordion>

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
