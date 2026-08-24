// SPDX-License-Identifier: GPL-3.0-or-later

import createEmscriptenModule from "../build/wired_web_client.mjs";
import { createWiredWebClient } from "../code/web/wired_web_client_module.mjs";
import { createWiredWebContentModule } from "../code/web/wired_web_content.mjs";

const result = document.querySelector("#result");
const canvas = document.querySelector("#wired-canvas");
const publish = (status, detail) => {
  result.dataset.status = status;
  result.textContent = `${status}\n${JSON.stringify(detail, null, 2)}`;
};
const captureEvidence = async (client, detail) => {
	if (!new URLSearchParams(location.search).has("capture")) return false;
	/* Arena receipt publication precedes the first stable gameplay camera by a
	 * few frames. Capture a bounded post-resize steady state, not that transient. */
	for (let settle = 0; settle < 120; ++settle)
		await new Promise(resolve => requestAnimationFrame(resolve));
	let frame = null, nonBackgroundPixels = 0, dynamicRange = 0, captureAttempts = 0;
	for (; captureAttempts < 120; ++captureAttempts) {
		const candidate = await client.aggregate.host.captureNextCanvas(2);
		if (candidate.width !== canvas.width || candidate.height !== canvas.height
				|| candidate.rgba.byteLength !== candidate.width * candidate.height * 4)
			throw new Error("WebGPU parity framebuffer readback is malformed");
		let minimum = 255, maximum = 0, changed = 0;
		const baseline = candidate.rgba[0] + candidate.rgba[1] + candidate.rgba[2];
		for (let offset = 0; offset < candidate.rgba.length; offset += 4) {
			const red = candidate.rgba[offset], green = candidate.rgba[offset + 1];
			const blue = candidate.rgba[offset + 2];
			const luma = (red * 54 + green * 183 + blue * 19) >>> 8;
			minimum = Math.min(minimum, luma); maximum = Math.max(maximum, luma);
			if (Math.abs(red + green + blue - baseline) > 36) ++changed;
		}
		dynamicRange = maximum - minimum; nonBackgroundPixels = changed;
		if (dynamicRange >= 16 && changed >= candidate.width * candidate.height / 200) {
			frame = candidate; break;
		}
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	if (!frame) throw new Error("WebGPU parity capture remained blank for 120 presented frames");
	detail.capturePasses = frame.capturePasses;
	detail.captureAttempts = captureAttempts + 1;
	detail.captureDynamicRange = dynamicRange;
	detail.captureNonBackgroundPixels = nonBackgroundPixels;
	const copy = document.createElement("canvas");
	copy.width = frame.width; copy.height = frame.height;
	const context = copy.getContext("2d");
	if (!context) throw new Error("2D capture context is unavailable");
	context.putImageData(new ImageData(new Uint8ClampedArray(frame.rgba), frame.width, frame.height), 0, 0);
	const blob = await new Promise(resolve => copy.toBlob(resolve, "image/png"));
	if (!blob) throw new Error("WebGPU parity PNG encoding failed");
	const imageResponse = await fetch("/__wired_parity__/webgpu/arena17.png", {
		method: "POST", headers: { "Content-Type": "image/png" }, body: blob
	});
	const metadataResponse = await fetch("/__wired_parity__/webgpu/arena17.meta.json", {
		method: "POST", headers: { "Content-Type": "application/json" },
		body: JSON.stringify({ ...detail, backend: "webgpu", map: "arena17",
			userAgent: navigator.userAgent,
			hostReceipt: client.aggregate.host.receipt(2) })
	});
	if (!imageResponse.ok || !metadataResponse.ok)
		throw new Error("WebGPU parity evidence upload failed");
	return true;
};
const waitForState = async (client) => {
  for (let frame = 0; frame < 300; ++frame) {
    const state = client.state();
    if (state === "running" || state === "failed") return state;
    await new Promise(resolve => requestAnimationFrame(resolve));
  }
  return "timeout";
};
const waitForArena = async (client) => {
	let arenaFlags = 0;
  for (let frame = 0; frame < 1800; ++frame) {
	arenaFlags = client.aggregate.module._WiredWeb_ArenaReceiptProbe?.() ?? 0;
	if (arenaFlags === 31 || client.state() === "failed") return arenaFlags;
	await new Promise(resolve => requestAnimationFrame(resolve));
  }
  return arenaFlags;
};
const waitForAuthored = async (client) => {
	let flags = 0;
	for (let frame = 0; frame < 600; ++frame) {
		flags = client.aggregate.module._WiredWeb_AuthoredReceiptProbe?.() ?? 0;
		if (flags === 31 || client.state() === "failed") return flags;
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return flags;
};
const waitForResize = async (client, width, height) => {
	for (let frame = 0; frame < 600; ++frame) {
		if (client.resize({ width, height })) {
			for (let settle = 0; settle < 5; ++settle)
				await new Promise(resolve => requestAnimationFrame(resolve));
			const arena = client.aggregate.module._WiredWeb_ArenaReceiptProbe?.() ?? 0;
			return client.state() === "running" && canvas.width === width
				&& canvas.height === height
				&& (arena & 15) === 15
				&& (client.aggregate.module._WiredWeb_AuthoredReceiptProbe?.() ?? 0) === 31;
		}
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return false;
};

const loadContentModule = (options) => createWiredWebContentModule({
	moduleFactory: createEmscriptenModule,
	moduleOptions: options,
	manifestUrl: new URL("../build/browser-content/manifest.json", import.meta.url),
	publish: event => publish("LOADING", event)
});

try {
  const create = (generation) => createWiredWebClient({
    moduleFactory: loadContentModule,
    gpu: navigator.gpu,
    canvas,
    statusElement: result,
    generation,
    canvasIdentity: 0x7100 + generation
  });
  const first = await create(1);
  globalThis.wiredWebClientProbeFirst = first;
	const firstState = await waitForState(first);
	const firstContentFlags = first.aggregate.module._WiredWeb_ContentProbe?.() ?? 0;
	const firstArenaFlags = firstState === "running" ? await waitForArena(first) : 0;
	const firstAuthoredFlags = firstState === "running" ? await waitForAuthored(first) : 0;
	if (firstState !== "running" || firstContentFlags !== 7 || firstArenaFlags !== 31
			|| firstAuthoredFlags !== 31) {
    const capability = first.aggregate.capability();
    const visibleReason = result.textContent;
    publish(capability.status === "unsupported" ? "UNSUPPORTED" : "FAIL", {
      phase: "first-start", state: firstState, capability,
			clientStatus: first.aggregate.module._WiredWeb_ClientStatus?.(), firstContentFlags,
			firstArenaFlags, firstAuthoredFlags, visibleReason,
			dispatchFailure: first.aggregate.lastDispatchError?.() ?? "",
      failure: first.failure(),
      width: canvas.width, height: canvas.height, userAgent: navigator.userAgent
    });
  } else {
    await new Promise(resolve => requestAnimationFrame(resolve));
    first.destroy();
		const second = await create(2);
		const secondState = await waitForState(second);
		const secondContentFlags = second.aggregate.module._WiredWeb_ContentProbe?.() ?? 0;
		const secondArenaFlags = secondState === "running" ? await waitForArena(second) : 0;
		const secondAuthoredFlags = secondState === "running" ? await waitForAuthored(second) : 0;
		const resized = secondState === "running" ? await waitForResize(second, 1600, 900) : false;
		const restored = resized ? await waitForResize(second, 1280, 720) : false;
		if (secondState === "running" && secondContentFlags === 7
				&& secondArenaFlags === 31
				&& secondAuthoredFlags === 31
				&& resized && restored
				&& canvas.width === 1280 && canvas.height === 720) {
		const passDetail = { schemaVersion: 1, width: canvas.width, height: canvas.height,
			firstState, secondState, firstContentFlags, secondContentFlags,
			firstArenaFlags, secondArenaFlags, firstAuthoredFlags, secondAuthoredFlags,
			resizeSequence: [[1280, 720], [1600, 900], [1280, 720]],
			deterministicReload: true,
		lifecycle: ["Com_Init", "Com_Frame", "Com_Shutdown"],
		renderer: "WebGPU RAL", content: second.aggregate.module.wiredWebContentReceipt,
		userAgent: navigator.userAgent };
		passDetail.captureRecorded = await captureEvidence(second, passDetail);
		publish("PASS", passDetail);
    } else {
      publish("FAIL", { phase: "reload", firstState, secondState,
        resized, restored, capability: second.aggregate.capability(),
		dispatchFailure: second.aggregate.lastDispatchError?.() ?? "",
		width: canvas.width, height: canvas.height });
    }
    globalThis.wiredWebClientProbe = second;
  }
} catch (error) {
  publish("FAIL", { message: String(error?.message ?? error), stack: String(error?.stack ?? ""),
    width: canvas.width, height: canvas.height });
}
