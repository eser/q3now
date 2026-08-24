// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import { createRalWebGpuBrowserModule,
  RAL_WEBGPU_BROWSER_DEFAULT_MODE } from "../render/ral/backends/webgpu/ral_webgpu_browser_module.mjs";

export const WIRED_WEB_CLIENT_MODULE_SCHEMA_VERSION = 1;

const terminal = new Set(["unsupported", "failed", "lost"]);

export async function createWiredWebClient({ moduleFactory, gpu, canvas,
    statusElement, requestAnimationFrame = globalThis.requestAnimationFrame?.bind(globalThis),
    cancelAnimationFrame = globalThis.cancelAnimationFrame?.bind(globalThis),
    eventTarget = globalThis, generation = 1, canvasIdentity = 1,
    displayMode = RAL_WEBGPU_BROWSER_DEFAULT_MODE } = {}) {
  if (typeof requestAnimationFrame !== "function" || typeof cancelAnimationFrame !== "function")
    throw new TypeError("browser animation-frame ownership is required");
  if (!statusElement || !("textContent" in statusElement))
    throw new TypeError("a visible capability status element is required");

  const aggregate = await createRalWebGpuBrowserModule({ moduleFactory, gpu, canvas,
    generation, canvasIdentity, displayMode });
  const module = aggregate.module;
  const startClient = module?._WiredWeb_ClientStart;
  const frameClient = module?._WiredWeb_ClientFrame;
  const stopClient = module?._WiredWeb_ClientShutdown;
  if (typeof startClient !== "function" || typeof frameClient !== "function"
      || typeof stopClient !== "function") {
    aggregate.destroy();
    throw new TypeError("Emscripten full-client lifecycle exports are unavailable");
  }

  let state = "starting", started = false, destroyed = false, frameHandle = 0;
  let lastFailure = "", pendingResize = null;
  const listeners = [];
  const publish = (next, reason = "") => {
    state = next;
    statusElement.hidden = next === "running";
    statusElement.textContent = reason || (next === "running" ? "" : `Wired Web client: ${next}`);
    if (statusElement.dataset) statusElement.dataset.wiredStatus = next;
  };
  const listen = (type, handler) => {
    if (typeof eventTarget?.addEventListener !== "function") return;
    eventTarget.addEventListener(type, handler); listeners.push([type, handler]);
  };
  listen("keydown", (event) => module._WiredWeb_InputKey?.(event.keyCode ?? 0, 1));
  listen("keyup", (event) => module._WiredWeb_InputKey?.(event.keyCode ?? 0, 0));
  listen("keypress", (event) => module._WiredWeb_InputChar?.(event.charCode ?? 0));
  listen("pointermove", (event) => module._WiredWeb_InputMouse?.(
    Number(event.movementX ?? 0), Number(event.movementY ?? 0)));

  const fail = (reason) => {
    if (state === "failed" && lastFailure) return;
    const message = String(reason?.message ?? reason ?? "Wired Web client failed closed");
    lastFailure = String(reason?.stack ?? message);
    publish("failed", message);
    if (started) { stopClient(); started = false; }
  };
  const priorFatal = globalThis.wiredWebClientFatal;
  globalThis.wiredWebClientFatal = (message) => fail(String(message || "Wired Web client terminated"));

  const tick = (timestamp) => {
    if (destroyed) return;
    try {
      const rendererStatus = aggregate.rendererPoll();
      const capability = aggregate.capability();
      if (terminal.has(capability.status)) {
        fail(capability.reason || `WebGPU ${capability.status}`); return;
      }
      if (rendererStatus === 3 || rendererStatus === 4 || rendererStatus <= 0) {
        fail(aggregate.lastDispatchError?.()
          || `WebGPU renderer failed with status ${rendererStatus}`); return;
      }
      /* A submitted WebGPU product frame remains authoritative until
       * queue.onSubmittedWorkDone resolves. Do not open the next frontend frame
       * while the renderer reports PENDING; that would violate the backend's
       * single in-flight frame contract. */
      if (rendererStatus === 1) {
        frameHandle = requestAnimationFrame(tick); return;
      }
      if (capability.status === "ready") {
		if (!started) {
          if (!startClient()) { fail("Wired Web client rejected startup"); return; }
			started = true; publish("running");
		}
		if (pendingResize) {
			if (!aggregate.resize?.(pendingResize)) {
				frameHandle = requestAnimationFrame(tick); return;
			}
			pendingResize = null;
		}
		if (!frameClient(timestamp)) { fail("Wired Web client frame boundary failed"); return; }
      }
      frameHandle = requestAnimationFrame(tick);
    } catch (error) { fail(error); }
  };
  publish("starting");
  frameHandle = requestAnimationFrame(tick);

  return Object.freeze({ schemaVersion: WIRED_WEB_CLIENT_MODULE_SCHEMA_VERSION,
    aggregate, state() { return state; }, failure() { return lastFailure; },
    resize(nextMode) {
		const width = Number(nextMode?.width), height = Number(nextMode?.height);
		if (destroyed || state !== "running" || typeof aggregate.resize !== "function"
				|| !Number.isInteger(width) || !Number.isInteger(height)
				|| width <= 0 || height <= 0) return false;
		pendingResize = Object.freeze({ width, height }); return true;
    },
    destroy() {
      if (destroyed) return; destroyed = true;
      cancelAnimationFrame(frameHandle);
      for (const [type, handler] of listeners) eventTarget.removeEventListener?.(type, handler);
      if (started) { stopClient(); started = false; }
      aggregate.destroy();
      globalThis.wiredWebClientFatal = priorFatal;
      publish("stopped");
    }
  });
}
