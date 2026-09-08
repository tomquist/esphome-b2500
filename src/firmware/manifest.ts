/**
 * Helpers to turn the manifest.json produced by the ESPHome build action into
 * an esp-web-tools manifest that points at locally extracted firmware files.
 */

export interface FirmwarePart {
  path: string;
  offset: number;
  [key: string]: unknown;
}

export interface FirmwareBuild {
  chipFamily: string;
  parts: FirmwarePart[];
  [key: string]: unknown;
}

export interface FirmwareManifest {
  name: string;
  version: string;
  home_assistant_domain?: string;
  new_install_prompt_erase?: boolean;
  builds: FirmwareBuild[];
  [key: string]: unknown;
}

export interface ManifestMetadata {
  /** Display name shown in the installer dialog. */
  name: string;
  /** Version shown in the installer dialog, usually the ESPHome version. */
  version: string;
}

export class InvalidManifestError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'InvalidManifestError';
  }
}

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

const parseParts = (value: unknown): FirmwarePart[] => {
  if (!Array.isArray(value)) {
    return [];
  }
  return value.flatMap((part) => {
    if (!isRecord(part) || typeof part.path !== 'string') {
      return [];
    }
    const offset = typeof part.offset === 'number' ? part.offset : 0;
    return [{ ...part, path: part.path, offset } as FirmwarePart];
  });
};

const parseBuild = (value: unknown): FirmwareBuild | null => {
  if (!isRecord(value) || typeof value.chipFamily !== 'string') {
    return null;
  }
  const parts = parseParts(value.parts);
  if (parts.length === 0) {
    return null;
  }
  return { ...value, chipFamily: value.chipFamily, parts } as FirmwareBuild;
};

/**
 * Accepts both the "partial" manifest (a single build without the surrounding
 * envelope) and the "complete" esp-web-tools manifest and always returns a
 * complete one.
 */
export const normalizeManifest = (
  raw: unknown,
  metadata: ManifestMetadata
): FirmwareManifest => {
  if (!isRecord(raw)) {
    throw new InvalidManifestError('The firmware manifest is not an object.');
  }

  const rawBuilds = Array.isArray(raw.builds) ? raw.builds : [raw];
  const builds = rawBuilds.flatMap((build) => {
    const parsed = parseBuild(build);
    return parsed ? [parsed] : [];
  });

  if (builds.length === 0) {
    throw new InvalidManifestError(
      'The firmware manifest does not contain a flashable build. The firmware ' +
        'was probably built for a platform that cannot be flashed from the browser.'
    );
  }

  return {
    home_assistant_domain: 'esphome',
    new_install_prompt_erase: false,
    ...raw,
    name: typeof raw.name === 'string' ? raw.name : metadata.name,
    version: typeof raw.version === 'string' ? raw.version : metadata.version,
    builds,
  };
};

/** Strips any directory prefix from a path found in a manifest or ZIP entry. */
export const fileName = (path: string): string =>
  path.split(/[\\/]/).pop() ?? path;

/** All firmware files referenced by the manifest, without directory prefixes. */
export const manifestFileNames = (manifest: FirmwareManifest): string[] => {
  const names = manifest.builds.flatMap((build) =>
    build.parts.map((part) => fileName(part.path))
  );
  return Array.from(new Set(names));
};

/**
 * Replaces every part path with the URL returned by `resolve`, so the manifest
 * can reference blob URLs of the extracted firmware files.
 */
export const withResolvedPaths = (
  manifest: FirmwareManifest,
  resolve: (name: string) => string | undefined
): FirmwareManifest => ({
  ...manifest,
  builds: manifest.builds.map((build) => ({
    ...build,
    parts: build.parts.map((part) => {
      const resolved = resolve(fileName(part.path));
      if (!resolved) {
        throw new InvalidManifestError(
          `The firmware archive does not contain "${fileName(part.path)}".`
        );
      }
      return { ...part, path: resolved };
    }),
  })),
});
