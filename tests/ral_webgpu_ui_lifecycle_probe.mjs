// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
// Real output pixels through the renderer's normal UI pipeline; no fake world,
// injected resources, alternative shader or exposure bypass in the host.
export async function verifyUiLifecycle(runtime) {
  const check = (ok, message) => { if (!ok) throw new Error(message); };
  const wait = async poll => {
    let status = 1;
    for (let i = 0; i < 240 && status === 1; ++i) {
      status = poll();
      if (status === 1) await new Promise(resolve => setTimeout(resolve, 16));
    }
    check(status === 2, `UI lifecycle poll=${status}: ${runtime.module.wiredWebModuleLog.slice(-4)} ${runtime.lastDispatchError()}`);
  };
  const samples = [];
  let baseline;
  const frame = async (name, submit) => {
    const captured = runtime.host.captureNextCanvas(runtime.capability().generation);
    check(submit() === 1, `UI lifecycle ${name} rejected: ${runtime.module.wiredWebModuleLog.slice(-4)}`);
    await wait(() => runtime.module._WiredWebGpu_UiSmokePoll());
    const image = await captured;
    check(image.width === 1280 && image.height === 720, "UI lifecycle extent mismatch");
    const pixel = (x, y) => [...image.rgba.slice((y * image.width + x) * 4, (y * image.width + x) * 4 + 3)];
    const inside = pixel(96, 64), outside = pixel(640, 360);
    check(inside.every((v, c) => Math.abs(v - [64, 128, 255][c]) <= 1), `${name} wrong UI color: ${inside}`);
    check(outside.every(v => v === 0), `${name} stale scene pixels: ${outside}`);
    if (baseline) check(image.rgba.every((v, i) => v === baseline[i]), `${name} changed UI output`);
    else baseline = image.rgba.slice();
    samples.push({name, inside, outside});
  };
  try {
    await frame("cold-registration", () => runtime.module._WiredWebGpu_RendererSmokeBegin(1));
    runtime.module._WiredWebGpu_ExposureSmokeEnd();
    await frame("resident-before-first-scene", () => runtime.module._WiredWebGpu_RendererSmokeBegin(2));
    // Prove the resident world remains drawable, then cover the UI-only pause.
    check(runtime.module._WiredWebGpu_ExposureSmokeFrame(0, 0) === 1, "world resume rejected");
    await wait(() => runtime.module._WiredWebGpu_ExposureSmokePoll());
    await frame("resident-after-scene", () => runtime.module._WiredWebGpu_UiSmokeFrame(0));
    await frame("level-unload-loading", () => runtime.module._WiredWebGpu_UiSmokeFrame(1));
    await frame("new-registration", () => runtime.module._WiredWebGpu_UiSmokeFrame(2));
  } finally {
    runtime.module._WiredWebGpu_ExposureSmokeEnd();
  }
  return {width: 1280, height: 720, frames: samples.length, samples,
    checks: ["exact-output-identity", "no-stale-world", "world-resume", "no-scene-metering", "no-sky-history-mutation"]};
}
