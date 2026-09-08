import type { FileEntry } from '@zip.js/zip.js';
import {
  FirmwareManifest,
  ManifestMetadata,
  fileName,
  manifestFileNames,
  normalizeManifest,
  withResolvedPaths,
} from './manifest';

/**
 * zip.js is only needed once a build finished, so it is loaded on demand to
 * keep it out of the initial bundle. Decompression runs on the main thread:
 * the firmware archive is small and this avoids spawning a web worker.
 */
const loadZipJs = async () => {
  const zip = await import('@zip.js/zip.js');
  zip.configure({ useWebWorkers: false });
  return zip;
};

export class WrongPasswordError extends Error {
  constructor() {
    super(
      'The firmware archive could not be decrypted with the build password.'
    );
    this.name = 'WrongPasswordError';
  }
}

export interface FirmwareBundle {
  /** Blob URL of a manifest whose parts point at the extracted binaries. */
  manifestUrl: string;
  manifest: FirmwareManifest;
  chipFamily: string;
  /** Revokes all blob URLs created for this bundle. */
  release: () => void;
}

export interface DownloadOptions {
  signal?: AbortSignal;
  /** Progress between 0 and 1, only called when the size is known. */
  onProgress?: (progress: number) => void;
}

export const downloadFirmwareArchive = async (
  url: string,
  { signal, onProgress }: DownloadOptions = {}
): Promise<Blob> => {
  const response = await fetch(url, { cache: 'no-store', signal });
  if (!response.ok) {
    throw new Error(
      `Downloading the firmware failed with HTTP status ${response.status}.`
    );
  }
  const totalHeader = Number(response.headers.get('content-length'));
  const total = Number.isFinite(totalHeader) ? totalHeader : 0;
  if (!response.body || total <= 0) {
    return response.blob();
  }

  const reader = response.body.getReader();
  const chunks: Uint8Array[] = [];
  let received = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    chunks.push(value);
    received += value.length;
    onProgress?.(Math.min(received / total, 1));
  }
  return new Blob(chunks as BlobPart[]);
};

// zip.js reports both a wrong and a missing password through these messages.
const isWrongPassword = (error: unknown) =>
  error instanceof Error &&
  /invalid password|encrypted entry/i.test(error.message);

/**
 * Decrypts the password protected firmware archive in the browser and exposes
 * its content as blob URLs that esp-web-tools can flash.
 */
export const extractFirmwareBundle = async (
  archive: Blob,
  password: string,
  metadata: ManifestMetadata
): Promise<FirmwareBundle> => {
  const { BlobReader, Uint8ArrayWriter, ZipReader } = await loadZipJs();
  const reader = new ZipReader(new BlobReader(archive), { password });
  const objectUrls: string[] = [];
  const release = () => {
    objectUrls.forEach((url) => URL.revokeObjectURL(url));
    objectUrls.length = 0;
  };

  try {
    const entries = await reader.getEntries();
    const entriesByName = new Map(
      entries
        .filter((entry): entry is FileEntry => !entry.directory)
        .map((entry) => [fileName(entry.filename), entry])
    );

    const readEntry = async (name: string): Promise<Uint8Array> => {
      const entry = entriesByName.get(name);
      if (!entry) {
        throw new Error(`The firmware archive does not contain "${name}".`);
      }
      return entry.getData<Uint8Array>(new Uint8ArrayWriter());
    };

    const manifestJson = new TextDecoder().decode(
      await readEntry('manifest.json')
    );
    const manifest = normalizeManifest(JSON.parse(manifestJson), metadata);

    const urlsByName: Record<string, string> = {};
    for (const name of manifestFileNames(manifest)) {
      const data = await readEntry(name);
      const url = URL.createObjectURL(
        new Blob([data as BlobPart], { type: 'application/octet-stream' })
      );
      objectUrls.push(url);
      urlsByName[name] = url;
    }

    const resolved = withResolvedPaths(manifest, (name) => urlsByName[name]);
    const manifestUrl = URL.createObjectURL(
      new Blob([JSON.stringify(resolved)], { type: 'application/json' })
    );
    objectUrls.push(manifestUrl);

    return {
      manifestUrl,
      manifest: resolved,
      chipFamily: resolved.builds[0].chipFamily,
      release,
    };
  } catch (error) {
    release();
    throw isWrongPassword(error) ? new WrongPasswordError() : error;
  } finally {
    await reader.close();
  }
};
