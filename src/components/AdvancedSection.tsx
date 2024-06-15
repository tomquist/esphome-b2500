import React from 'react';
import {
  Accordion,
  AccordionSummary,
  AccordionDetails,
  TextField,
  Typography,
  Box,
  Grid,
} from '@mui/material';
import { ExpandMore } from '@mui/icons-material';
import BooleanField from './BooleanField';
import { FormValues } from '../types';

interface AdvancedSectionProps {
  formValues: FormValues;
  handleInputChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handleAutoRestartChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handleSetWifiChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handleWebServerChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handleOtaChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handleFallbackHotspotChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handleManualIPChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handlePowerzeroChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  handlePowermeterChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
}

const AdvancedSection: React.FC<AdvancedSectionProps> = ({
  formValues,
  handleInputChange,
  handleAutoRestartChange,
  handleSetWifiChange,
  handleWebServerChange,
  handleOtaChange,
  handleFallbackHotspotChange,
  handleManualIPChange,
  handlePowerzeroChange,
  handlePowermeterChange,
}) => {
  let setWifiSsidInvalid = formValues.set_wifi.ssid.trim() === '';
  let setWifiPasswordInvalid = formValues.set_wifi.password.trim() === '';
  let webserverPortInvalid =
    !formValues.web_server.port ||
    formValues.web_server.port < 1 ||
    formValues.web_server.port > 65535;
  let otaPasswordInvalid = formValues.ota.password.trim() === '';
  let fallbackHotspotSsidInvalid =
    formValues.fallback_hotspot.ssid.trim() === '';
  let powermeterTxPinInvalid = formValues.powermeter.tx_pin.trim() === '';
  let powermeterRxPinInvalid = formValues.powermeter.rx_pin.trim() === '';
  let powermeterBaudRateInvalid = formValues.powermeter.baud_rate < 0;
  let powermeterStopBitsInvalid =
    formValues.powermeter.stop_bits !== 1 &&
    formValues.powermeter.stop_bits !== 2;
  let manualIpAddressInvalid = formValues.manual_ip.ip.trim() === '';
  let manualIpGatewayInvalid = formValues.manual_ip.gateway.trim() === '';
  let manualIpSubnetInvalid = formValues.manual_ip.subnet.trim() === '';
  let manualIpDnsInvalid = formValues.manual_ip.dns.trim() === '';
  let powerzeroGridPowerTopicInvalid =
    formValues.powerzero.grid_power_topic.trim() === '';
  let powerzeroLimitCmdTopic =
    formValues.powerzero.limit_cmd_topic.trim() === '';
  let powerzerolimitStateTopicInvalid =
    formValues.powerzero.limit_state_topic.trim() === '';
  return (
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
            required
            error={formValues.auto_restart.restart_after_error_count < 1}
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
          prop="enable_set_wifi"
          label="Enable Set Wifi"
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
              required
              error={setWifiSsidInvalid}
              helperText={setWifiSsidInvalid ? 'SSID is required' : ''}
            />
            <TextField
              label="Password"
              name="password"
              type="password"
              value={formValues.set_wifi.password}
              onChange={handleSetWifiChange}
              fullWidth
              margin="normal"
              required
              error={setWifiPasswordInvalid}
              helperText={setWifiPasswordInvalid ? 'Password is required' : ''}
            />
          </Box>
        )}

        <BooleanField
          value={formValues}
          onChange={handleInputChange}
          prop="enable_enforce_dod"
          label="Enable Enforce DoD"
        />
        <BooleanField
          value={formValues}
          onChange={handleInputChange}
          prop="enable_web_server"
          label="Enable Web Server"
        />
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
                  required
                  error={webserverPortInvalid}
                />
              </Grid>
              <Grid item xs={12} sm={6}>
                <BooleanField
                  value={formValues.web_server}
                  prop={'ota'}
                  onChange={handleWebServerChange}
                  label={'OTA'}
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
                  helperText={
                    <>
                      Note that the file must be in the same directory as the
                      config file. If you intend to use the automatic build
                      system, this must be set to <code>./v2/www.js</code>.
                    </>
                  }
                />
              </Grid>
            </Grid>
          </Box>
        )}

        <BooleanField
          value={formValues}
          onChange={handleInputChange}
          prop="enable_ota"
          label="Enable OTA"
        />
        {formValues.enable_ota && (
          <Box mb={2} border={1} borderRadius={5} padding={2}>
            <TextField
              label="Password"
              name="password"
              type="password"
              value={formValues.ota.password}
              onChange={handleOtaChange}
              fullWidth
              margin="normal"
              required
              error={otaPasswordInvalid}
              helperText={otaPasswordInvalid ? 'Password is required' : ''}
            />
            <BooleanField
              value={formValues.ota}
              onChange={handleOtaChange}
              prop="enable_unprotected_writes"
              label="Enable Unprotected Writes"
            />
          </Box>
        )}

        <BooleanField
          value={formValues}
          onChange={handleInputChange}
          prop="enable_fallback_hotspot"
          label="Enable Fallback Hotspot"
        />
        {formValues.enable_fallback_hotspot && (
          <TextField
            label="Fallback Hotspot SSID"
            name="ssid"
            value={formValues.fallback_hotspot.ssid}
            onChange={handleFallbackHotspotChange}
            fullWidth
            margin="normal"
            required
            error={fallbackHotspotSsidInvalid}
            helperText={fallbackHotspotSsidInvalid ? 'SSID is required' : ''}
          />
        )}
        <BooleanField
          value={formValues}
          onChange={handleInputChange}
          prop="enable_cmd30"
          label="Enable CMD30"
        />
        <BooleanField
          value={formValues}
          onChange={handleInputChange}
          prop="enable_esp_temperature"
          label="Enable ESP temperature"
        />
        <BooleanField
          value={formValues}
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
                  required
                  error={powermeterTxPinInvalid}
                  helperText={
                    powermeterTxPinInvalid ? 'TX Pin is required' : ''
                  }
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
                  required
                  error={powermeterRxPinInvalid}
                  helperText={
                    powermeterRxPinInvalid ? 'RX Pin is required' : ''
                  }
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
                  error={powermeterBaudRateInvalid}
                  helperText={
                    powermeterBaudRateInvalid ? 'Baud Rate is invalid' : ''
                  }
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
                  error={powermeterStopBitsInvalid}
                  helperText={
                    powermeterStopBitsInvalid ? 'Stop Bits is invalid' : ''
                  }
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
                  required
                  error={manualIpAddressInvalid}
                  helperText={
                    manualIpAddressInvalid ? 'IP Address is required' : ''
                  }
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
                  required
                  error={manualIpGatewayInvalid}
                  helperText={
                    manualIpGatewayInvalid ? 'Gateway is required' : ''
                  }
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
                  required
                  error={manualIpSubnetInvalid}
                  helperText={manualIpSubnetInvalid ? 'Subnet is required' : ''}
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
                  required
                  error={manualIpDnsInvalid}
                  helperText={manualIpDnsInvalid ? 'DNS is required' : ''}
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
                  required
                  error={powerzeroGridPowerTopicInvalid}
                  helperText={
                    powerzeroGridPowerTopicInvalid
                      ? 'Grid Power Topic is required'
                      : ''
                  }
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
                  required
                  error={powerzeroLimitCmdTopic}
                  helperText={
                    powerzeroLimitCmdTopic ? 'Limit CMD Topic is required' : ''
                  }
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
                  required
                  error={powerzerolimitStateTopicInvalid}
                  helperText={
                    powerzerolimitStateTopicInvalid
                      ? 'Limit State Topic is required'
                      : ''
                  }
                />
              </Grid>
            </Grid>
          </Box>
        )}
      </AccordionDetails>
    </Accordion>
  );
};

export default AdvancedSection;
