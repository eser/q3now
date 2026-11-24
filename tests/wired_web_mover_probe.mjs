// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// WebGPU TAA object motion on a real mover (TASK-257). The browser client starts e1m5 (four looping Quake 1 func_trains) with TAA
// on, puts the camera straight down onto two trains (the view of tests/ral-metal-vulkan-visual-parity.sh's e1m5 entry) and reads
// the renderer's "taa entity motion" lines: rigid entities must record poses, find their committed previous pose and draw their
// object velocity into the TAA effect mask. Serve the repository root with tests/ral-webgpu-vulkan-parity-server.py and open
// /tests/wired_web_mover_probe.html (keep the tab visible, or shim requestAnimationFrame: hidden tabs do not tick).

import createEmscriptenModule from "../build/wired_web_client.mjs";
import { createWiredWebClient } from "../code/web/wired_web_client_module.mjs";
import { createWiredWebContentModule } from "../code/web/wired_web_content.mjs";

const result = document.querySelector("#result");
const canvas = document.querySelector("#wired-canvas");
const publish = (status, detail) => {
	result.dataset.status = status;
	result.textContent = `${status}\n${JSON.stringify(detail, null, 2)}`;
};
const nextFrame = () => new Promise(resolve => requestAnimationFrame(resolve));
const waitFrames = async (count) => { for (let i = 0; i < count; ++i) await nextFrame(); };

/* Runs console text through the client's command buffer (the launch-argument buffer doubles as a command line at run time). */
const execute = (client, text) => {
	const module = client.aggregate.module;
	const pointer = module._WiredWeb_CommandBuffer?.();
	const size = module._WiredWeb_CommandBufferSize?.() ?? 0;
	const memory = module.wasmMemory?.buffer ?? module.HEAPU8?.buffer;
	const bytes = new TextEncoder().encode(String(text));
	if (!pointer || !memory || bytes.length + 1 > size) return false;
	const view = new Uint8Array(memory, pointer, size);
	view.set(bytes); view[bytes.length] = 0;
	return module._WiredWeb_ExecuteCommandBuffer?.() === 1;
};

const parseMotion = (line) => {
	const values = {};
	for (const [, key, value] of line.matchAll(/(\w+)=(\d+)/g)) values[key] = Number(value);
	return values;
};

/* The engine log reaches the page through console.log (the renderer's lines on the renderer.init channel); keep the TAA lines. */
const engineLines = [];
const consoleLog = console.log.bind(console);
console.log = (...args) => {
	const text = args.map(String).join(" ");
	if (text.includes("taa entity motion") || text.includes("taa frames")) engineLines.push(text);
	consoleLog(...args);
};

try {
	const client = await createWiredWebClient({
		moduleFactory: (options) => createWiredWebContentModule({
			moduleFactory: createEmscriptenModule,
			moduleOptions: options,
			manifestUrl: new URL("../build/browser-content/manifest.json", import.meta.url),
			publish: event => publish("LOADING", event)
		}),
		gpu: navigator.gpu, canvas, statusElement: result, generation: 1, canvasIdentity: 0x7200,
		launchArguments: "+log renderer.init info +set s_initsound 0 +set sv_cheats 1 +set r_aa 1 +set cg_drawGun 0 +set cg_draw2D 0 +map e1m5"
	});
	globalThis.wiredWebMoverProbe = { client, phase: "starting" };
	let state = "starting";
	for (let frame = 0; frame < 3600 && state !== "running" && state !== "failed"; ++frame) {
		state = client.state(); await nextFrame();
	}
	if (state !== "running") throw new Error(`client state ${state}`);
	globalThis.wiredWebMoverProbe.phase = "running";
	await waitFrames(900);   /* "running" precedes the map load: let the level and the player spawn first */
	const placed = execute(client, "cmd noclip; cmd setviewpos -1360 2150 400 90 89");
	globalThis.wiredWebMoverProbe.phase = "placed";
	/* The renderer reports every 100 TAA frames; a page frame is not a game frame, so wait on the report itself (up to 120 s). */
	const reportsBefore = engineLines.filter(line => line.includes("taa entity motion")).length;
	for (const deadline = performance.now() + 120000; performance.now() < deadline;) {
		if (engineLines.filter(line => line.includes("taa entity motion")).length >= reportsBefore + 2) break;
		await nextFrame();
	}
	const log = [...(client.aggregate.module.wiredWebModuleLog ?? []), ...engineLines];
	const motionLines = log.filter(line => line.includes("taa entity motion"));
	const last = motionLines.length ? parseMotion(motionLines[motionLines.length - 1]) : null;
	const detail = { placed, motionLines: motionLines.slice(-4), taaLines: log.filter(line => line.includes("taa frames")).slice(-2), last };
	globalThis.wiredWebMoverProbe = { client, detail, phase: "done" };
	publish(last && last.recorded > 0 && last.previous > 0 && last.draws > 0 ? "PASS" : "FAIL", detail);
} catch (error) {
	publish("FAIL", { message: String(error?.message ?? error), stack: String(error?.stack ?? "") });
}
