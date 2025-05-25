import template from './template.jinja2';
import templateV2 from './template_v2.jinja2';
import templateV2Minimal from './template_v2_minimal.jinja2';

interface TemplateCapability {
  readonly requiresStorageVersion: boolean;
  readonly requiresStorageId: boolean;
  readonly hasStorage: boolean;
  readonly canUseNativeAPI: boolean;
  readonly canDisableMQTT: boolean;
  readonly canChangeLoglevel: boolean;
  readonly canDefinePollInterval: boolean;
  readonly hasAutoRestart: boolean;
  readonly canEnableHexdump: boolean;
  readonly canEnableCellquery: boolean;
  readonly canEnableTimerQuery: boolean;
  readonly canEnableExperimentalCommands: boolean;
  readonly canEnableEnforceDod: boolean;
  readonly canEnableCmd30: boolean;
  readonly canDefineJsInclude: boolean;
  readonly hasPowerZeroScript: boolean;
}

interface Template {
  readonly name: string;
  readonly description: string;
  readonly template: string;
  readonly capabilities: TemplateCapability;
}

export const templates = {
  v1: {
    name: 'Legacy',
    description: "Old config based on noone2k's ESPHome config",
    template,
    capabilities: {
      requiresStorageVersion: true,
      requiresStorageId: false,
      hasStorage: true,
      canUseNativeAPI: false,
      canDisableMQTT: false,
      canChangeLoglevel: false,
      canDefinePollInterval: true,
      hasAutoRestart: true,
      canEnableHexdump: true,
      canEnableCellquery: true,
      canEnableTimerQuery: true,
      canEnableExperimentalCommands: true,
      canEnableEnforceDod: true,
      canEnableCmd30: true,
      canDefineJsInclude: true,
      hasPowerZeroScript: true,
    },
  },
  v2: {
    name: 'Native ESPHome component',
    description:
      'A new config that uses a native ESPHome component. Slightly less resource-intensive and more stable.',
    template: templateV2,
    capabilities: {
      requiresStorageVersion: true,
      requiresStorageId: false,
      hasStorage: true,
      canUseNativeAPI: true,
      canDisableMQTT: true,
      canChangeLoglevel: true,
      canDefinePollInterval: true,
      hasAutoRestart: false,
      canEnableHexdump: false,
      canEnableCellquery: false,
      canEnableTimerQuery: false,
      canEnableExperimentalCommands: false,
      canEnableEnforceDod: false,
      canEnableCmd30: false,
      canDefineJsInclude: true,
      hasPowerZeroScript: true,
    },
  },
  'v2-minimal': {
    name: 'Minimal native ESPHome component',
    description:
      'A minimal version of the native ESPHome component. Exposes storage data through a smaller number of MQTT topics without using ESPHome sensors. Use this for low-power devices or if you manually integrate the storage into your home automation system using MQTT.',
    template: templateV2Minimal,
    capabilities: {
      requiresStorageVersion: true,
      requiresStorageId: false,
      hasStorage: true,
      canUseNativeAPI: false,
      canDisableMQTT: false,
      canChangeLoglevel: true,
      canDefinePollInterval: true,
      hasAutoRestart: false,
      canEnableHexdump: false,
      canEnableCellquery: false,
      canEnableTimerQuery: false,
      canEnableExperimentalCommands: false,
      canEnableEnforceDod: false,
      canEnableCmd30: false,
      canDefineJsInclude: false,
      hasPowerZeroScript: true,
    },
  },
} as const satisfies Record<string, Template>;
