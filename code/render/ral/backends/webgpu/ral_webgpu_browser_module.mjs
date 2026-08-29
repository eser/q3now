// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import { createRalWebGpuBrowserHost } from "./ral_webgpu_browser_host.mjs";
import { installRalWebGpuBrowserDispatch } from "./ral_webgpu_browser_dispatch.mjs";

export const RAL_WEBGPU_BROWSER_MODULE_SCHEMA_VERSION = 1;
export const RAL_WEBGPU_BROWSER_DEFAULT_MODE = Object.freeze({ width: 1280, height: 720 });

const capability = (generation, status, reason = "") => Object.freeze({
  schemaVersion: RAL_WEBGPU_BROWSER_MODULE_SCHEMA_VERSION,
  generation, status, reason, supported: status === "ready"
});

export async function createRalWebGpuBrowserModule({ moduleFactory, gpu, canvas,
    devicePixelRatio = () => 1, generation = 1, canvasIdentity = 1,
    displayMode = RAL_WEBGPU_BROWSER_DEFAULT_MODE } = {}) {
  if (typeof moduleFactory !== "function" || !canvas
      || !Number.isSafeInteger(generation) || generation <= 0
      || !Number.isSafeInteger(canvasIdentity) || canvasIdentity <= 0) {
    throw new TypeError("moduleFactory, canvas and positive identities are required");
  }
  const width = Number(displayMode?.width), height = Number(displayMode?.height);
  if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) {
    throw new TypeError("display mode must contain a positive integer extent");
  }
  canvas.width = width; canvas.height = height;
  const host = createRalWebGpuBrowserHost({ gpu, canvas, devicePixelRatio });
	const moduleLog = [];
	const captureLog = (stream, value) => {
		moduleLog.push(`${stream}: ${String(value ?? "")}`);
		if (moduleLog.length > 256) moduleLog.splice(0, moduleLog.length - 256);
	};
	const module = await moduleFactory({ canvas, noInitialRun: true,
		print: value => captureLog("stdout", value),
		printErr: value => captureLog("stderr", value) });
	Object.defineProperty(module, "wiredWebModuleLog", {
		value: moduleLog, enumerable: true
	});
  const memory = () => {
    const value = module.wasmMemory?.buffer ?? module.HEAPU8?.buffer;
    if (!(value instanceof ArrayBuffer)
        && !(typeof SharedArrayBuffer !== "undefined" && value instanceof SharedArrayBuffer)) {
      throw new TypeError("Emscripten module linear memory is unavailable");
    }
    return value;
  };
  const installation = installRalWebGpuBrowserDispatch({ host, memory,
    canvasIdentity });
  const start = module._RalWebGpu_BrowserModuleStart;
  const modulePoll = module._RalWebGpu_BrowserModulePoll;
  const getRefApi = module._GetRefAPI;
  const rendererPoll = module._WiredWebGpu_RendererPoll;
  const rendererResize = module._WiredWebGpu_RendererResize;
  if (typeof start !== "function" || typeof modulePoll !== "function"
      || typeof getRefApi !== "function"
      || typeof rendererPoll !== "function" || typeof rendererResize !== "function") {
    installation.uninstall();
    throw new TypeError("Emscripten WebGPU renderer module exports are unavailable");
  }
  const accepted = start(generation, canvasIdentity, width, height);
  if (!accepted) {
    installation.uninstall();
    const detail = typeof installation.bridge.lastError === "function"
      ? installation.bridge.lastError() : "";
    const failed = capability(generation, "failed", detail
      ? `WebGPU aggregate runtime rejected startup: ${detail}`
      : "WebGPU aggregate runtime rejected startup");
    return Object.freeze({ host, module, mode: Object.freeze({ width, height }),
      capability() { return failed; },
      destroy() {} });
  }
  let destroyed = false, moduleStatus = 1;
  return Object.freeze({ host, module, mode: Object.freeze({ width, height }),
    rendererPoll() {
      moduleStatus = modulePoll();
      return moduleStatus === 2 ? rendererPoll() : moduleStatus;
    },
    resize(nextMode) {
      const nextWidth = Number(nextMode?.width), nextHeight = Number(nextMode?.height);
      if (destroyed || moduleStatus !== 2 || !Number.isInteger(nextWidth)
          || !Number.isInteger(nextHeight) || nextWidth <= 0 || nextHeight <= 0) return false;
      return rendererResize(nextWidth, nextHeight) === 1;
    },
    lastDispatchError() { let hostError = "";
      try { hostError = host.receipt(generation).error; } catch { /* teardown */ }
      return [...new Set([installation.bridge.lastError(),
        installation.bridge.lastFailure(), hostError].filter(Boolean))].join("\n"); },
    capability() {
      try {
        const receipt = host.receipt(generation);
        if (receipt.ready) {
          if (moduleStatus === 2) return capability(generation, "ready");
          if (moduleStatus === 0) return capability(generation, "failed",
            installation.bridge.lastError() || installation.bridge.lastFailure()
              || "WebGPU aggregate runtime failed during startup");
          return capability(generation, "starting");
        }
        if (receipt.status === "unsupported") return capability(generation, "unsupported",
          receipt.reason || "WebGPU is unavailable");
        if (receipt.status === "lost") return capability(generation, "lost",
          receipt.lost?.message || receipt.reason || "WebGPU device lost");
        return capability(generation, receipt.status, receipt.reason);
      } catch { return capability(generation, "starting"); }
    },
    destroy() {
      if (destroyed) return; destroyed = true;
      module._RalWebGpu_BrowserModuleStop?.(); installation.uninstall();
      try { host.destroy(generation); } catch { /* aggregate teardown may own it */ }
    }
  });
}

export async function recreateRalWebGpuBrowserModule(previous, options = {}) {
  if (!previous || typeof previous.destroy !== "function") throw new TypeError("previous module is required");
  const prior = typeof previous.capability === "function" ? previous.capability() : previous.capability;
  const generation = Number(prior?.generation ?? 0) + 1;
  previous.destroy(); return createRalWebGpuBrowserModule({ ...options, generation });
}
