import "@testing-library/jest-dom/vitest";
import { afterEach, vi } from "vitest";
import { cleanup } from "@testing-library/react";
afterEach(cleanup);
globalThis.ResizeObserver = class {
  observe() {}
  disconnect() {}
  unobserve() {}
};
Object.defineProperty(HTMLDialogElement.prototype, "showModal", {
  value: function () {
    this.open = true;
  },
});
Object.defineProperty(HTMLDialogElement.prototype, "close", {
  value: function () {
    this.open = false;
  },
});
vi.stubGlobal(
  "WebSocket",
  class {
    close() {}
  },
);
