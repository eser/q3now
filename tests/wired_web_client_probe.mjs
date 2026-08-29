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
const captureEvidence = async (client, detail, stem = "arena17") => {
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
	const imageResponse = await fetch(`/__wired_parity__/webgpu/${stem}.png`, {
		method: "POST", headers: { "Content-Type": "image/png" }, body: blob
	});
	const metadataResponse = await fetch(`/__wired_parity__/webgpu/${stem}.meta.json`, {
		method: "POST", headers: { "Content-Type": "application/json" },
		body: JSON.stringify({ ...detail, backend: "webgpu", map: "arena17", surface: stem,
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
const waitForMenuState = async (client, visible) => {
	for (let frame = 0; frame < 180; ++frame) {
		const receipt = client.aggregate.module._WiredWeb_UiMenuReceiptProbe?.() ?? -1;
		if (visible ? (receipt & 7) === 7 : (receipt & 7) === 4)
			return true;
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return false;
};
const waitForMenuFocus = async (client, predicate) => {
	let receipt = client.aggregate.module._WiredWeb_UiMenuReceiptProbe?.() ?? -1;
	for (let frame = 0; frame < 180; ++frame) {
		if (predicate((receipt >>> 16) & 0xffff)) return receipt;
		await new Promise(resolve => requestAnimationFrame(resolve));
		receipt = client.aggregate.module._WiredWeb_UiMenuReceiptProbe?.() ?? -1;
	}
	return receipt;
};
const exerciseAuthoredRoots = async (client) => {
	const count = client.aggregate.module._WiredWeb_UiRootCount?.() ?? 0;
	const receipts = [];
	if (count !== 25) return { passed: false, count, receipts };
	for (let index = 0; index < count; ++index) {
		if ((client.aggregate.module._WiredWeb_UiActivateRoot?.(index) ?? 0) !== 1)
			return { passed: false, count, failedIndex: index, receipts };
		let receipt = 0;
		for (let frame = 0; frame < 120; ++frame) {
			receipt = client.aggregate.module._WiredWeb_UiRootReceiptProbe?.(index) ?? 0;
			if ((receipt & 7) === 7) break;
			await new Promise(resolve => requestAnimationFrame(resolve));
		}
		receipts.push(receipt >>> 0);
		if ((receipt & 7) !== 7)
			return { passed: false, count, failedIndex: index, receipts };
	}
	return { passed: true, count, receipts };
};
const waitForHud = async (client) => {
	for (let frame = 0; frame < 180; ++frame) {
		const flags = client.aggregate.module._WiredWeb_UiHudReceiptProbe?.() ?? 0;
		if (flags === 7) return flags;
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return client.aggregate.module._WiredWeb_UiHudReceiptProbe?.() ?? 0;
};
const hudExtent = (client) => {
	const packed = client.aggregate.module._WiredWeb_UiHudExtentProbe?.() ?? 0;
	return [packed >>> 16, packed & 0xffff];
};
const waitForHudExtent = async (client, width, height) => {
	for (let frame = 0; frame < 180; ++frame) {
		const [drawWidth, drawHeight] = hudExtent(client);
		if (drawWidth === width && drawHeight === height) return true;
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return false;
};
const waitForServerReceipt = async (client, mask, expected) => {
	let receipt = 0;
	for (let frame = 0; frame < 240; ++frame) {
		receipt = client.aggregate.module._WiredWeb_UiServerReceiptProbe?.() ?? 0;
		if ((receipt & mask) === expected) return receipt;
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return receipt;
};
const waitForLayerReceipt = async (client, mask, expected) => {
	let receipt = 0;
	for (let frame = 0; frame < 240; ++frame) {
		receipt = client.aggregate.module._WiredWeb_UiLayerReceiptProbe?.() ?? 0;
		if ((receipt & mask) === expected) return receipt;
		await new Promise(resolve => requestAnimationFrame(resolve));
	}
	return receipt;
};
const dispatchBrowserKey = (type, keyCode, key, code) => {
	const event = new KeyboardEvent(type, { key, code, bubbles: true });
	Object.defineProperty(event, "keyCode", { value: keyCode });
	globalThis.dispatchEvent(event);
};
const exerciseConsoleAndLoading = async (client) => {
	const trace = {};
	client.aggregate.module._WiredWeb_UiActivateLayerFixture?.(1);
	trace.loading = await waitForLayerReceipt(client, 0x13, 0x13);
	if ((trace.loading & 0x13) !== 0x13) return { passed: false, ...trace };
	trace.loadingCaptureRecorded = await captureEvidence(client, trace, "loading");
	client.aggregate.module._WiredWeb_UiActivateLayerFixture?.(0);
	trace.loadingClosed = await waitForLayerReceipt(client, 0x01, 0x00);
	dispatchBrowserKey("keydown", 192, "`", "Backquote");
	dispatchBrowserKey("keyup", 192, "`", "Backquote");
	trace.console = await waitForLayerReceipt(client, 0x0c, 0x0c);
	if ((trace.console & 0x0c) !== 0x0c) return { passed: false, ...trace };
	for (let frame = 0; frame < 45; ++frame)
		await new Promise(resolve => requestAnimationFrame(resolve));
	trace.consoleCaptureRecorded = await captureEvidence(client, trace, "console");
	dispatchBrowserKey("keydown", 192, "`", "Backquote");
	dispatchBrowserKey("keyup", 192, "`", "Backquote");
	trace.consoleClosed = await waitForLayerReceipt(client, 0x04, 0x00);
	return { passed: (trace.loadingClosed & 0x01) === 0
		&& (trace.consoleClosed & 0x04) === 0, ...trace };
};
const exerciseServerBrowser = async (client) => {
	const trace = {};
	client.aggregate.module._WiredWeb_UiEnableServerFixture?.(2);
	client.aggregate.module._WiredWeb_UiActivateRoot?.(18);
	trace.opened = await waitForServerReceipt(client, 0x13, 0x13);
	if ((trace.opened & 0x13) !== 0x13) return { passed: false, ...trace };
	/* Exercise the browser event bridge itself, not an internal state setter. */
	globalThis.dispatchEvent(new WheelEvent("wheel", { deltaY: 120 }));
	trace.scrolled = await waitForServerReceipt(client, 0x17, 0x17);
	if ((trace.scrolled & 0x17) !== 0x17) return { passed: false, ...trace };
	/* The wheel receipt is synchronous, while Clay publishes rendered geometry
	 * at frame end. Wait for that public presentation boundary before querying
	 * the canonical row rectangle. */
	for (let settle = 0; settle < 2; ++settle)
		await new Promise(resolve => requestAnimationFrame(resolve));
	const bounds = canvas.getBoundingClientRect();
	const packedRowCenter = client.aggregate.module._WiredWeb_UiServerRowCenterProbe?.() ?? 0;
	const rowX = packedRowCenter >>> 16, rowY = packedRowCenter & 0xffff;
	trace.rowCenter = [rowX, rowY];
	if (!rowX || !rowY || rowX >= canvas.width || rowY >= canvas.height)
		return { passed: false, ...trace };
	/* Derive the click from canonical native Clay/listbox geometry, then
	 * exercise the public browser pointer bridge. Guessing viewport fractions
	 * made the gate dependent on responsive filter/header height. */
	for (const xOffset of [0, -8, 8]) {
		const clientX = bounds.left + (rowX + xOffset) * bounds.width / canvas.width;
		const clientY = bounds.top + rowY * bounds.height / canvas.height;
		globalThis.dispatchEvent(new PointerEvent("pointermove", { clientX, clientY }));
		await new Promise(resolve => requestAnimationFrame(resolve));
		globalThis.dispatchEvent(new PointerEvent("pointerdown", { clientX, clientY, buttons: 1 }));
		await new Promise(resolve => requestAnimationFrame(resolve));
		globalThis.dispatchEvent(new PointerEvent("pointerup", { clientX, clientY, buttons: 0 }));
		for (let settle = 0; settle < 3; ++settle)
			await new Promise(resolve => requestAnimationFrame(resolve));
		trace.clicked = client.aggregate.module._WiredWeb_UiServerReceiptProbe?.() ?? 0;
		if ((trace.clicked & 0x1f) === 0x1f) {
			trace.clickedCanvas = [rowX + xOffset, rowY];
			break;
		}
	}
	if ((trace.clicked & 0x1f) !== 0x1f)
		return { passed: false, ...trace };
	trace.layoutPerformanceGate = "wiredui_clay_layout_benchmark";
	trace.captureRecorded = await captureEvidence(client, trace, "servers");
	client.aggregate.module._WiredWeb_InputKey?.(27, 1);
	client.aggregate.module._WiredWeb_InputKey?.(27, 0);
	trace.serverClosedReceipt = await waitForServerReceipt(client, 0x1, 0x0);
	trace.serverClosed = (trace.serverClosedReceipt & 0x1) === 0;
	client.aggregate.module._WiredWeb_UiActivateRoot?.(9);
	trace.returnedToMain = await waitForMenuState(client, true);
	return { passed: trace.serverClosed && trace.returnedToMain, ...trace };
};
const exerciseMenuToggle = async (client) => {
	const trace = {};
	trace.authoredRoots = await exerciseAuthoredRoots(client);
	if (!trace.authoredRoots.passed) return { passed: false, ...trace };
	client.aggregate.module._WiredWeb_UiActivateRoot?.(9);
	if (!(await waitForMenuState(client, true))) return { passed: false, ...trace };
	trace.initial = client.aggregate.module._WiredWeb_UiMenuReceiptProbe?.() ?? -1;
	const initialFocus = (trace.initial >>> 16) & 0xffff;
	/* keycodes.h freezes K_UPARROW/K_DOWNARROW at 132/133. Prove that the
	 * authored main-root focus owner, not a visual-only highlight, consumes them. */
	dispatchBrowserKey("keydown", 40, "ArrowDown", "ArrowDown");
	dispatchBrowserKey("keyup", 40, "ArrowDown", "ArrowDown");
	trace.afterDown = await waitForMenuFocus(client,
		focus => (focus & 0x8000) !== 0 && focus !== initialFocus);
	dispatchBrowserKey("keydown", 38, "ArrowUp", "ArrowUp");
	dispatchBrowserKey("keyup", 38, "ArrowUp", "ArrowUp");
	trace.afterUp = await waitForMenuFocus(client, focus => focus === initialFocus);
	trace.navigationPassed = (initialFocus & 0x8000) !== 0
		&& ((trace.afterDown >>> 16) & 0xffff) !== initialFocus
		&& ((trace.afterUp >>> 16) & 0xffff) === initialFocus;
	if (!trace.navigationPassed) return { passed: false, ...trace };
	trace.serverBrowser = await exerciseServerBrowser(client);
	if (!trace.serverBrowser.passed) return { passed: false, ...trace };
	trace.layerMatrix = await exerciseConsoleAndLoading(client);
	if (!trace.layerMatrix.passed) return { passed: false, ...trace };
	client.aggregate.module._WiredWeb_UiActivateRoot?.(-1);
	const closed = await waitForMenuState(client, false);
	trace.afterClose = client.aggregate.module._WiredWeb_UiMenuReceiptProbe?.() ?? -1;
	if (!closed) return { passed: false, ...trace };
	trace.hudReceipt = await waitForHud(client);
	if (trace.hudReceipt !== 7) return { passed: false, ...trace };
	trace.initialHudExtent = hudExtent(client);
	trace.resized = await waitForResize(client, 1600, 900)
		&& await waitForHudExtent(client, 1600, 900);
	trace.resizedHudExtent = hudExtent(client);
	trace.restored = trace.resized && await waitForResize(client, 1280, 720)
		&& await waitForHudExtent(client, 1280, 720);
	trace.restoredHudExtent = hudExtent(client);
	if (!trace.resized || !trace.restored) return { passed: false, ...trace };
	const gameplayCapture = {
		hudReceipt: trace.hudReceipt,
		initialHudExtent: trace.initialHudExtent,
		resizedHudExtent: trace.resizedHudExtent,
		restoredHudExtent: trace.restoredHudExtent,
		crosshairContract: "procedural-visible-centered"
	};
	trace.gameplayCaptureRecorded = await captureEvidence(client, gameplayCapture, "arena17");
	trace.gameplayCapture = gameplayCapture;
	client.aggregate.module._WiredWeb_InputKey?.(27, 1);
	client.aggregate.module._WiredWeb_InputKey?.(27, 0);
	const opened = await waitForMenuState(client, true);
	trace.afterOpen = client.aggregate.module._WiredWeb_UiMenuReceiptProbe?.() ?? -1;
	return { passed: opened, ...trace };
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
	const firstUiTextFlags = first.aggregate.module._WiredWeb_UiTextReceiptProbe?.() ?? 0;
	if (firstState !== "running" || firstContentFlags !== 7 || firstArenaFlags !== 31
			|| firstAuthoredFlags !== 31 || firstUiTextFlags !== 31) {
    const capability = first.aggregate.capability();
    const visibleReason = result.textContent;
    publish(capability.status === "unsupported" ? "UNSUPPORTED" : "FAIL", {
      phase: "first-start", state: firstState, capability,
			clientStatus: first.aggregate.module._WiredWeb_ClientStatus?.(), firstContentFlags,
			firstArenaFlags, firstAuthoredFlags, firstUiTextFlags, visibleReason,
			dispatchFailure: first.aggregate.lastDispatchError?.() ?? "",
			rendererFailureStage: first.aggregate.module._WiredWebGpu_RendererFailureStage?.() ?? -1,
			moduleLog: first.aggregate.module.wiredWebModuleLog?.slice(-80) ?? [],
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
		const secondUiTextFlags = second.aggregate.module._WiredWeb_UiTextReceiptProbe?.() ?? 0;
		const menuToggle = secondState === "running" ? await exerciseMenuToggle(second) : { passed: false };
		if (secondState === "running" && secondContentFlags === 7
				&& secondArenaFlags === 31
				&& secondAuthoredFlags === 31
				&& secondUiTextFlags === 31
				&& menuToggle.passed
				&& canvas.width === 1280 && canvas.height === 720) {
		const passDetail = { schemaVersion: 1, width: canvas.width, height: canvas.height,
			firstState, secondState, firstContentFlags, secondContentFlags,
			firstArenaFlags, secondArenaFlags, firstAuthoredFlags, secondAuthoredFlags,
			firstUiTextFlags, secondUiTextFlags,
			menuToggleRoundTrip: menuToggle,
			resizeSequence: menuToggle.passed ? [[1280, 720], [1600, 900], [1280, 720]] : [],
			deterministicReload: true,
		lifecycle: ["Com_Init", "Com_Frame", "Com_Shutdown"],
		renderer: "WebGPU RAL", content: second.aggregate.module.wiredWebContentReceipt,
		userAgent: navigator.userAgent };
		passDetail.menuCaptureRecorded = await captureEvidence(second, passDetail, "menu");
		publish("PASS", passDetail);
    } else {
      publish("FAIL", { phase: "reload", firstState, secondState,
		firstUiTextFlags, secondUiTextFlags, menuToggleRoundTrip: menuToggle,
		capability: second.aggregate.capability(),
		dispatchFailure: second.aggregate.lastDispatchError?.() ?? "",
		rendererFailureStage: second.aggregate.module._WiredWebGpu_RendererFailureStage?.() ?? -1,
		width: canvas.width, height: canvas.height });
    }
    globalThis.wiredWebClientProbe = second;
  }
} catch (error) {
  publish("FAIL", { message: String(error?.message ?? error), stack: String(error?.stack ?? ""),
    width: canvas.width, height: canvas.height });
}
