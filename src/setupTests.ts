// jest-dom adds custom jest matchers for asserting on DOM nodes.
// allows you to do things like:
// expect(element).toHaveTextContent(/react/i)
// learn more: https://github.com/testing-library/jest-dom
import '@testing-library/jest-dom';
import { Blob as NodeBlob } from 'node:buffer';
import { TextDecoder as NodeTextDecoder } from 'node:util';
import * as streams from 'node:stream/web';

// jsdom does not implement the Streams API, which zip.js relies on.
const globals = globalThis as Record<string, unknown>;
for (const name of [
  'ReadableStream',
  'WritableStream',
  'TransformStream',
] as const) {
  if (globals[name] === undefined) {
    globals[name] = streams[name];
  }
}

// jsdom does not expose TextDecoder.
if (globals.TextDecoder === undefined) {
  globals.TextDecoder = NodeTextDecoder;
}

// jsdom's Blob implementation is missing arrayBuffer()/stream().
if (typeof Blob === 'undefined' || Blob.prototype.arrayBuffer === undefined) {
  globals.Blob = NodeBlob;
}
