import type { FileEntry } from '@zip.js/zip.js';
import {
  FirmwareManifest,
  ManifestMetadata,
  directoryName,
  fileName,
  manifestPartPaths,
  normalizePath,
  normalizeManifest,
  resolvePath,
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

/**
 * Looks archive entries up by the path a manifest refers to them with. Paths
 * are resolved against the directory the manifest lives in; a bare file name is
 * only accepted when exactly one entry carries it, so two firmware files with
 * the same name in different directories can never resolve to each other.
 */
export const createFileIndex = (entryNames: string[]) => {
  const byPath = new Map<string, string>();
  const byFileName = new Map<string, string | null>();
  for (const entryName of entryNames) {
    byPath.set(normalizePath(entryName), entryName);
    const name = fileName(entryName);
    byFileName.set(name, byFileName.has(name) ? null : entryName);
  }
  return (path: string, directory = ''): string | undefined =>
    byPath.get(resolvePath(directory, path)) ??
    byFileName.get(fileName(path)) ??
    undefined;
};

/**
 * Exposes the content of the firmware archive as blob URLs that esp-web-tools
 * can flash. The archive is a plain ZIP by the time it gets here:
 * `decryptFirmwareArchive` has already unwrapped what the bucket served.
 */
export const extractFirmwareBundle = async (
  archive: Blob,
  metadata: ManifestMetadata
): Promise<FirmwareBundle> => {
  const { BlobReader, Uint8ArrayWriter, ZipReader } = await loadZipJs();
  const reader = new ZipReader(new BlobReader(archive));
  const objectUrls: string[] = [];
  const release = () => {
    objectUrls.forEach((url) => URL.revokeObjectURL(url));
    objectUrls.length = 0;
  };

  try {
    const files = (await reader.getEntries()).filter(
      (entry): entry is FileEntry => !entry.directory
    );
    const entriesByName = new Map(
      files.map((entry) => [entry.filename, entry])
    );
    const findFile = createFileIndex(files.map((entry) => entry.filename));

    const readEntry = async (entryName: string): Promise<Uint8Array> => {
      const entry = entriesByName.get(entryName);
      if (!entry) {
        throw new Error(
          `The firmware archive does not contain "${entryName}".`
        );
      }
      return entry.getData<Uint8Array>(new Uint8ArrayWriter());
    };

    const manifestName = findFile('manifest.json');
    if (!manifestName) {
      throw new Error('The firmware archive does not contain a manifest.json.');
    }
    const manifestJson = new TextDecoder().decode(
      await readEntry(manifestName)
    );
    const manifest = normalizeManifest(JSON.parse(manifestJson), metadata);

    // Manifest paths are relative to the manifest itself.
    const manifestDirectory = directoryName(manifestName);
    const urlsByPath: Record<string, string> = {};
    for (const path of manifestPartPaths(manifest)) {
      const entryName = findFile(path, manifestDirectory);
      if (!entryName) {
        continue;
      }
      const data = await readEntry(entryName);
      const url = URL.createObjectURL(
        new Blob([data as BlobPart], { type: 'application/octet-stream' })
      );
      objectUrls.push(url);
      urlsByPath[path] = url;
    }

    const resolved = withResolvedPaths(manifest, (path) => urlsByPath[path]);
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
    throw error;
  } finally {
    await reader.close();
  }
};
