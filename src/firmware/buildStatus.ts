/**
 * The build workflow publishes a small JSON status document next to the sealed
 * firmware archive. The web UI polls it to follow the build without requiring
 * the user to watch the GitHub Actions run.
 */

const S3_BUCKET = process.env.REACT_APP_S3_BUCKET;
const AWS_REGION = process.env.REACT_APP_AWS_REGION;

const objectUrl = (key: string) =>
  `https://${S3_BUCKET}.s3.${AWS_REGION}.amazonaws.com/${key}`;

// `.zip.enc` rather than `.zip`: the object is a ZIP sealed to the key pair the
// page generated for this build, not a ZIP any unzip tool can open.
export const firmwareDownloadUrl = (identifier: string) =>
  objectUrl(`firmware/${identifier}.zip.enc`);

export const buildStatusUrl = (identifier: string) =>
  objectUrl(`firmware/${identifier}.status.json`);

/**
 * One slice of the build output. Segments are immutable and numbered from
 * zero; the status document says how many of them exist. S3 cannot append to
 * an object, so a growing log has to be published as a run of objects like
 * this - see scripts/publish-build-log.sh.
 */
export const buildLogSegmentUrl = (identifier: string, sequence: number) =>
  objectUrl(`firmware/${identifier}.log.${sequence}`);

export const buildListUrl = 'https://github.com/tomquist/esphome-b2500/actions';

export type BuildState = 'building' | 'success' | 'error';

/** What a running build is currently doing. */
export type BuildStep = 'preparing' | 'compiling' | 'packaging';

export interface BuildProgress {
  completed: number;
  /** Absent until the build knows how much work there is. */
  total?: number;
  /**
   * What the compiler is on right now - the source behind the object file
   * written most recently. Absent before the first one lands.
   */
  current?: string;
}

export interface BuildStatus {
  state: BuildState;
  /** Link to the GitHub Actions run that builds the firmware. */
  runUrl?: string;
  /** Error message, only set for failed builds. */
  message?: string;
  step?: BuildStep;
  /** Compile units finished so far, reported while compiling. */
  progress?: BuildProgress;
  /**
   * How many build output segments have been published. Absent while the
   * workflow has not managed to read its own job log.
   */
  logSegments?: number;
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

const STEPS: BuildStep[] = ['preparing', 'compiling', 'packaging'];

const optionalString = (value: unknown): string | undefined =>
  typeof value === 'string' && value.length > 0 ? value : undefined;

/**
 * The status document is fetched over the network, and the URLs in it end up in
 * `href` attributes and in `fetch()`. Accept only absolute https URLs on the
 * origins we publish to, so a document that ever came from somewhere else - or
 * was written by a build step with a rewritten environment - cannot turn a link
 * into `javascript:` or point the download at another host.
 */
const trustedUrl = (
  value: unknown,
  allowedOrigins: readonly string[]
): string | undefined => {
  const raw = optionalString(value);
  if (!raw) {
    return undefined;
  }
  try {
    const url = new URL(raw);
    // The parsed form, so what is returned is what was actually checked.
    return url.protocol === 'https:' && allowedOrigins.includes(url.origin)
      ? url.href
      : undefined;
  } catch (error) {
    return undefined;
  }
};

const originOf = (url: string): string | undefined => {
  try {
    return new URL(url).origin;
  } catch (error) {
    return undefined;
  }
};

const bucketOrigins = (): readonly string[] => {
  const origin = originOf(objectUrl(''));
  return origin ? [origin] : [];
};

const GITHUB_ORIGINS = ['https://github.com'] as const;

const optionalCount = (value: unknown): number | undefined =>
  typeof value === 'number' && Number.isFinite(value) && value >= 0
    ? value
    : undefined;

/**
 * Caps on what the build output can make the page do. The workflow publishes
 * far less than either; they are here so a status document that ever said
 * otherwise cannot send the page fetching forever or hand the log view
 * something too big to render.
 */
const MAX_LOG_SEGMENTS = 500;
const MAX_SEGMENT_CHARS = 200000;
/**
 * And on the segments once joined: capping each one still leaves the sum of
 * them unbounded, and it is the accumulated string that the log view re-renders
 * every time a segment arrives.
 */
const MAX_LOG_CHARS = 1000000;

/**
 * Segment bodies are whatever the compiler wrote, so they arrive with stray
 * control bytes in them. React renders the result as text either way - this is
 * so what the user sees is what the build printed, rather than a `\r` eating
 * the line it was on.
 *
 * The workflow strips terminal escapes before publishing, so the CSI pass here
 * is the second of two. It earns its place by being the last one: dropping the
 * escape byte alone would leave `[1:2m` sitting in the page as text, and this
 * is the only point that sees what a segment actually contains.
 */
export const cleanLogText = (raw: string): string => {
  const text = raw
    .replace(/\r\n?/g, '\n')
    // Parameter bytes, then intermediates, then the final byte: the whole CSI
    // form rather than the colour codes alone.
    // eslint-disable-next-line no-control-regex
    .replace(/\u001b\[[0-?]*[ -/]*[@-~]/g, '')
    // eslint-disable-next-line no-control-regex
    .replace(/[\u0000-\u0008\u000b-\u001f\u007f]/g, '');
  return text.length > MAX_SEGMENT_CHARS
    ? text.slice(-MAX_SEGMENT_CHARS)
    : text;
};

/** How many segments the status document says exist, clamped to the cap. */
/**
 * Adds a segment to what the page has already read, keeping the end: the tail
 * is where a build that is still running says what it is doing, and where one
 * that stopped says why.
 */
export const appendLogText = (previous: string, segment: string): string => {
  const joined = previous + segment;
  return joined.length > MAX_LOG_CHARS ? joined.slice(-MAX_LOG_CHARS) : joined;
};

/** How many segments the status document says exist, clamped to the cap. */
const parseSegmentCount = (value: unknown): number | undefined => {
  const count = optionalCount(value);
  return count !== undefined && Number.isInteger(count) && count > 0
    ? Math.min(count, MAX_LOG_SEGMENTS)
    : undefined;
};

const parseStep = (value: unknown): BuildStep | undefined => {
  const step = optionalString(value)?.toLowerCase();
  return STEPS.find((known) => known === step);
};

/**
 * A file name from the build tree, shown as a line of text on the page. Long
 * enough for anything ESP-IDF compiles, short enough that a status document
 * saying otherwise cannot push the rest of the line off screen.
 */
const MAX_CURRENT_CHARS = 80;

const parseCurrent = (value: unknown): string | undefined => {
  const raw = optionalString(value);
  if (!raw) {
    return undefined;
  }
  // eslint-disable-next-line no-control-regex
  const name = raw.replace(/[\u0000-\u001f\u007f]/g, '').trim();
  return name.length > 0 && name.length <= MAX_CURRENT_CHARS ? name : undefined;
};

const parseProgress = (value: unknown): BuildProgress | undefined => {
  if (typeof value !== 'object' || value === null) {
    return undefined;
  }
  const record = value as Record<string, unknown>;
  const completed = optionalCount(record.completed);
  if (completed === undefined) {
    return undefined;
  }
  const total = optionalCount(record.total);
  return {
    completed,
    // A total that cannot be reached would only ever show a stuck bar.
    total: total && total >= completed ? total : undefined,
    current: parseCurrent(record.current),
  };
};

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
    runUrl: trustedUrl(record.run_url, GITHUB_ORIGINS),
    message: optionalString(record.message),
    step: parseStep(record.step),
    progress: parseProgress(record.progress),
    logSegments: parseSegmentCount(record.log_segments),
    firmwareUrl: trustedUrl(record.firmware_url, bucketOrigins()),
    firmwareName: optionalString(record.firmware_name),
    esphomeVersion: optionalString(record.esphome_version),
  };
};

/**
 * Reads one segment of the build output. Resolves with `null` when it is not
 * published yet, which is how a page that read a segment count from a status
 * document written moments ago tells "not there" from "not any more".
 */
export const fetchLogSegment = async (
  identifier: string,
  sequence: number,
  signal?: AbortSignal
): Promise<string | null> => {
  // No `no-store` here, unlike the status document: a segment never changes
  // once published, so a reload may reuse whatever the browser kept.
  const response = await fetch(buildLogSegmentUrl(identifier, sequence), {
    signal,
  });
  if (!response.ok) {
    return null;
  }
  return cleanLogText(await response.text());
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
  /** Gives up on a single request that never completes. */
  requestTimeoutMs?: number;
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

class RequestTimeoutError extends Error {
  constructor() {
    super('Timed out while reading the build status.');
    this.name = 'RequestTimeoutError';
  }
}

/**
 * Runs a single status request under its own timeout. Without it a request
 * that never settles would keep the poll loop from reaching its deadline.
 */
const fetchOnce = async (
  fetchStatus: typeof fetchBuildStatus,
  identifier: string,
  timeoutMs: number,
  signal?: AbortSignal
): Promise<BuildStatus | null> => {
  const controller = new AbortController();
  const abort = () => controller.abort();
  signal?.addEventListener('abort', abort, { once: true });
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    const request = fetchStatus(identifier, controller.signal);
    // Keeps the abort below from surfacing as an unhandled rejection.
    request.catch(() => {});
    return await Promise.race([
      request,
      new Promise<never>((_, reject) => {
        timer = setTimeout(() => {
          controller.abort();
          reject(new RequestTimeoutError());
        }, timeoutMs);
      }),
    ]);
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener('abort', abort);
  }
};

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
  intervalMs = 3000,
  timeoutMs = 30 * 60 * 1000,
  requestTimeoutMs = 30000,
  fetchStatus = fetchBuildStatus,
  sleep: wait = sleep,
}: PollOptions): Promise<BuildStatus> => {
  const deadline = Date.now() + timeoutMs;
  let consecutiveFailures = 0;
  for (;;) {
    let status: BuildStatus | null = null;
    try {
      status = await fetchOnce(
        fetchStatus,
        identifier,
        requestTimeoutMs,
        signal
      );
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
