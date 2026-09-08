/**
 * The build workflow publishes a small JSON status document next to the
 * firmware ZIP. The web UI polls it to follow the build without requiring the
 * user to watch the GitHub Actions run.
 */

const S3_BUCKET = process.env.REACT_APP_S3_BUCKET;
const AWS_REGION = process.env.REACT_APP_AWS_REGION;

const objectUrl = (key: string) =>
  `https://${S3_BUCKET}.s3.${AWS_REGION}.amazonaws.com/${key}`;

export const firmwareDownloadUrl = (identifier: string) =>
  objectUrl(`firmware/${identifier}.zip`);

export const buildStatusUrl = (identifier: string) =>
  objectUrl(`firmware/${identifier}.status.json`);

export const buildListUrl = 'https://github.com/tomquist/esphome-b2500/actions';

export type BuildState = 'building' | 'success' | 'error';

export interface BuildStatus {
  state: BuildState;
  /** Link to the GitHub Actions run that builds the firmware. */
  runUrl?: string;
  /** Error message, only set for failed builds. */
  message?: string;
  firmwareUrl?: string;
  /** Name of the firmware directory inside the ZIP, e.g. `b2500-esp32`. */
  firmwareName?: string;
  esphomeVersion?: string;
}

const STATES: Record<string, BuildState> = {
  queued: 'building',
  building: 'building',
  in_progress: 'building',
  success: 'success',
  completed: 'success',
  error: 'error',
  failed: 'error',
  failure: 'error',
  cancelled: 'error',
};

const optionalString = (value: unknown): string | undefined =>
  typeof value === 'string' && value.length > 0 ? value : undefined;

/**
 * Parses the status document. Returns `null` for anything we don't understand
 * so that a half-written or unrelated object is treated like "not there yet".
 */
export const parseBuildStatus = (raw: unknown): BuildStatus | null => {
  if (typeof raw !== 'object' || raw === null) {
    return null;
  }
  const record = raw as Record<string, unknown>;
  const status = optionalString(record.status)?.toLowerCase();
  const state = status ? STATES[status] : undefined;
  if (!state) {
    return null;
  }
  return {
    state,
    runUrl: optionalString(record.run_url),
    message: optionalString(record.message),
    firmwareUrl: optionalString(record.firmware_url),
    firmwareName: optionalString(record.firmware_name),
    esphomeVersion: optionalString(record.esphome_version),
  };
};

export class BuildTimeoutError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'BuildTimeoutError';
  }
}

/**
 * Fetches the status document. Resolves with `null` while the workflow hasn't
 * published it yet (the object doesn't exist) or when it cannot be read, e.g.
 * because the browser blocked the cross-origin request.
 */
export const fetchBuildStatus = async (
  identifier: string,
  signal?: AbortSignal
): Promise<BuildStatus | null> => {
  const response = await fetch(buildStatusUrl(identifier), {
    cache: 'no-store',
    signal,
  });
  if (!response.ok) {
    return null;
  }
  try {
    return parseBuildStatus(await response.json());
  } catch (error) {
    return null;
  }
};

export interface PollOptions {
  identifier: string;
  signal?: AbortSignal;
  /** Called whenever a new status was read, including repeated ones. */
  onStatus?: (status: BuildStatus) => void;
  /**
   * Called when the status document could not be fetched at all, with the
   * number of consecutive failures. Usually a blocked cross-origin request.
   */
  onFetchError?: (error: unknown, consecutiveFailures: number) => void;
  intervalMs?: number;
  timeoutMs?: number;
  fetchStatus?: typeof fetchBuildStatus;
  sleep?: (ms: number, signal?: AbortSignal) => Promise<void>;
}

export const sleep = (ms: number, signal?: AbortSignal) =>
  new Promise<void>((resolve, reject) => {
    if (signal?.aborted) {
      reject(new DOMException('Aborted', 'AbortError'));
      return;
    }
    const timer = setTimeout(() => {
      signal?.removeEventListener('abort', onAbort);
      resolve();
    }, ms);
    const onAbort = () => {
      clearTimeout(timer);
      reject(new DOMException('Aborted', 'AbortError'));
    };
    signal?.addEventListener('abort', onAbort, { once: true });
  });

/**
 * Polls the status document until the build finished, failed or the timeout
 * elapsed. Transient network errors are ignored, they are indistinguishable
 * from "the object does not exist yet".
 */
export const pollBuildStatus = async ({
  identifier,
  signal,
  onStatus,
  onFetchError,
  intervalMs = 5000,
  timeoutMs = 30 * 60 * 1000,
  fetchStatus = fetchBuildStatus,
  sleep: wait = sleep,
}: PollOptions): Promise<BuildStatus> => {
  const deadline = Date.now() + timeoutMs;
  let consecutiveFailures = 0;
  for (;;) {
    let status: BuildStatus | null = null;
    try {
      status = await fetchStatus(identifier, signal);
      consecutiveFailures = 0;
    } catch (error) {
      if (signal?.aborted) {
        throw error;
      }
      consecutiveFailures += 1;
      onFetchError?.(error, consecutiveFailures);
      status = null;
    }
    if (status) {
      onStatus?.(status);
      if (status.state !== 'building') {
        return status;
      }
    }
    if (Date.now() >= deadline) {
      throw new BuildTimeoutError(
        'Timed out while waiting for the build to finish.'
      );
    }
    await wait(intervalMs, signal);
  }
};
