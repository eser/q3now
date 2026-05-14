#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Regenerates code/renderer/ral_vulkan/pipeline_test_spv.h from the three
# pipeline_test_*.glsl shaders in this directory. Run after editing any GLSL.
# Requires glslangValidator (any Vulkan SDK 1.2+) and node on $PATH.

set -euo pipefail
cd "$(dirname "$0")"

# Default Vulkan SDK location on the dev image — override with VULKAN_SDK / PATH.
if ! command -v glslangValidator >/dev/null 2>&1; then
	if [ -d "/c/VulkanSDK/1.4.341.1/Bin" ]; then
		export PATH="/c/VulkanSDK/1.4.341.1/Bin:$PATH"
	fi
fi

NODE="${NODE:-node}"
if ! command -v "$NODE" >/dev/null 2>&1 && [ -x "/c/Program Files/nodejs/node.exe" ]; then
	NODE="/c/Program Files/nodejs/node.exe"
fi

glslangValidator -V pipeline_test_vert.glsl -S vert -o pipeline_test_vert.spv
glslangValidator -V pipeline_test_frag.glsl -S frag -o pipeline_test_frag.spv
glslangValidator -V pipeline_test_comp.glsl -S comp -o pipeline_test_comp.spv

"$NODE" -e '
const fs = require("fs");
function emit(name, path) {
	const buf = fs.readFileSync(path);
	if (buf.length % 4 !== 0) { console.error("not a u32 multiple"); process.exit(1); }
	const words = buf.length / 4;
	let out = "static const uint32_t " + name + "[" + words + "] = {\n";
	for (let i = 0; i < words; i++) {
		const w = buf.readUInt32LE(i * 4);
		if ((i % 6) === 0) out += "\t";
		out += "0x" + w.toString(16).padStart(8, "0") + "u,";
		out += ((i % 6) === 5 || i === words - 1) ? "\n" : " ";
	}
	out += "};\n";
	out += "static const uint32_t " + name + "_size = " + buf.length + ";\n\n";
	process.stdout.write(out);
}
process.stdout.write("// SPDX-License-Identifier: GPL-3.0-or-later\n");
process.stdout.write("// SPDX-FileCopyrightText: 2024-present Wired Engine contributors\n//\n");
process.stdout.write("// pipeline_test_spv.h -- auto-generated from test_shaders/pipeline_test_{vert,frag,comp}.glsl\n");
process.stdout.write("// via glslangValidator. Embedded for the \\ral_dump pipeline test (Phase 7.3c).\n");
process.stdout.write("// Regenerate with code/renderer/ral_vulkan/test_shaders/regen.sh after editing the GLSL.\n//\n");
process.stdout.write("// DO NOT EDIT by hand.\n\n");
process.stdout.write("#ifndef WIRED_RAL_VULKAN_PIPELINE_TEST_SPV_H\n");
process.stdout.write("#define WIRED_RAL_VULKAN_PIPELINE_TEST_SPV_H\n\n");
process.stdout.write("#include <stdint.h>\n\n");
emit("ral_pipeline_test_vert_spv", "pipeline_test_vert.spv");
emit("ral_pipeline_test_frag_spv", "pipeline_test_frag.spv");
emit("ral_pipeline_test_comp_spv", "pipeline_test_comp.spv");
process.stdout.write("#endif // WIRED_RAL_VULKAN_PIPELINE_TEST_SPV_H\n");
' > ../pipeline_test_spv.h

# Leave the .spv files in place — gitignored, easy to inspect with spirv-dis.
echo "[ral pipeline test] regenerated ../pipeline_test_spv.h"
