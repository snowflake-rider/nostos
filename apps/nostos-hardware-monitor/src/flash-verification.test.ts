import { describe, expect, test } from "bun:test";

import { findFirstByteMismatch } from "./debug-session.js";

describe("findFirstByteMismatch", () => {
  test("returns the first differing byte", () => {
    const expected = Uint8Array.from([0x00, 0x11, 0x22, 0x33]);
    const actual = Uint8Array.from([0x00, 0x11, 0xaa, 0xbb]);

    expect(findFirstByteMismatch(expected, actual)).toEqual({
      offset: 2,
      expected: 0x22,
      actual: 0xaa,
    });
  });

  test("returns undefined for equal byte arrays", () => {
    const bytes = Uint8Array.from([0xde, 0xad, 0xbe, 0xef]);
    expect(findFirstByteMismatch(bytes, bytes.slice())).toBeUndefined();
  });

  test("reports the boundary when lengths differ", () => {
    expect(
      findFirstByteMismatch(
        Uint8Array.from([0x01, 0x02, 0x03]),
        Uint8Array.from([0x01, 0x02]),
      ),
    ).toEqual({ offset: 2, expected: 0x03, actual: undefined });
  });
});
