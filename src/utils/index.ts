import crypto from 'crypto';
import { FormValues } from '../types';

export const defaultFormValues: FormValues = {
    name: 'b2500',
    friendly_name: 'B2500',
    mqtt: {
        topic: 'b2500',
        broker: 'mqttbroker.local',
        port: 1883,
        username: '',
        password: '',
        discovery: false,
    },
    wifi: {
        ssid: 'MyWifi',
        password: 'MyPassword',
    },
    board: 'esp32-c3-devkitm-1',
    enable_auto_restart: true,
    auto_restart: {
        restart_after_error_count: 8,
    },
    enable_cellquery: false,
    enable_cmd30: false,
    enable_esp_temperature: false,
    enable_powermeter: false,
    enable_experimental_commands: false,
    enable_hexdump: false,
    enable_set_wifi: false,
    set_wifi: {
        ssid: 'MyWifi',
        password: 'MyPassword',
    },
    enable_set_mqtt: false,
    powermeter: {
        tx_pin: 'GPIO6',
        rx_pin: 'GPIO7',
        baud_rate: 9600,
        stop_bits: 1,
    },
    enable_enforce_dod: false,
    enable_powerzero: false,
    powerzero: {
        grid_power_topic: 'tibber-esp/sensor/power/state',
        limit_cmd_topic: 'openDTU/XXXXXXXXXXXX/cmd/limit_persistent_relative',
        limit_state_topic: 'openDTU/XXXXXXXXXXXX/state/limit_relative',
    },
    enable_manual_ip: false,
    manual_ip: {
        ip: '192.168.1.100',
        gateway: '192.168.1.1',
        subnet: '255.255.255.0',
        dns: '192.168.1.1',
    },
    enable_web_server: true,
    web_server: {
        port: 80,
        ota: false,
        js_include: './v2/www.js',
    },
    enable_ota: true,
    ota: {
        password: 'my_ota_password',
        enable_unprotected_writes: false,
    },
    enable_fallback_hotspot: true,
    fallback_hotspot: {
        ssid: 'ESPHome-b2500'
    },
    storages: [
        { name: "B2500", version: 1, mac_address: "00:00:00:00:00:00" },
    ],
};

export const mergeDeep = (target: any, source: any) => {
    for (const key of Object.keys(source)) {
        if (source[key] instanceof Object) {
            if (!target[key]) {
                Object.assign(target, { [key]: {} });
            }
            mergeDeep(target[key], source[key]);
        } else {
            if (target[key] === undefined) {
                Object.assign(target, { [key]: source[key] });
            }
        }
    }
    return target;
};

export const getAllSecrets = (debouncedFormValues: FormValues) => {
    const secrets = [];
    if (debouncedFormValues.mqtt.password) {
        secrets.push(debouncedFormValues.mqtt.password);
    }
    if (debouncedFormValues.wifi.password) {
        secrets.push(debouncedFormValues.wifi.password);
    }
    if (debouncedFormValues.set_wifi.password) {
        secrets.push(debouncedFormValues.set_wifi.password);
    }
    if (debouncedFormValues.ota.password) {
        secrets.push(debouncedFormValues.ota.password);
    }
    for (const storage of debouncedFormValues.storages) {
        if (storage.mac_address) {
            secrets.push(storage.mac_address);
        }
    }
    return secrets;
};

export const generatePassword = () => {
    return Math.random().toString(36).slice(-8);
};

export const aesEncrypt = ({ password, input }: { password: string; input: Buffer }) => {
    const key = crypto.createHash('sha256').update(password).digest();
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);
    const encrypted = Buffer.concat([cipher.update(input), cipher.final()]);
    return Buffer.concat([iv, encrypted]);
};
