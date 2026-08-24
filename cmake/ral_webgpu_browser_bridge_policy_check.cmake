# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
foreach(FILE IN ITEMS ral_webgpu_browser_abi.h ral_webgpu_browser_bridge.h ral_webgpu_browser_bridge.c)
	file(READ "${BACKEND}/${FILE}" TEXT)
	foreach(FORBIDDEN IN ITEMS "WebGL2" "webgl2" "SDL_" "navigator.gpu" "GPUDevice" "GPUCanvasContext" "code/renderer" "code/renderer2")
		string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "browser-native object leaked across WASM ABI in ${FILE}: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
file(READ "${BACKEND}/ral_webgpu_browser_bridge.c" SOURCE)
foreach(NEEDLE IN ITEMS
	"RAL_WEBGPU_BROWSER_OP_BEGIN_ADAPTER"
	"RAL_WEBGPU_BROWSER_OP_POLL_ADAPTER"
	"RAL_WEBGPU_BROWSER_OP_BEGIN_DEVICE"
	"RAL_WEBGPU_BROWSER_OP_POLL_DEVICE"
	"RAL_WEBGPU_BROWSER_OP_POLL_DEVICE_LOSS"
	"RAL_WEBGPU_BROWSER_OP_CANVAS_CONFIGURE"
	"RAL_WEBGPU_BROWSER_OP_CANVAS_ACQUIRE"
	"RAL_WEBGPU_BROWSER_OP_CANVAS_PRESENT"
	"RAL_WEBGPU_BROWSER_OP_CREATE_BUFFER"
	"RAL_WEBGPU_BROWSER_OP_WRITE_TEXTURE"
	"RAL_WEBGPU_BROWSER_OP_BEGIN_ROUND_TRIP"
	"RAL_WEBGPU_BROWSER_OP_CREATE_SHADER_MODULE"
	"RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE"
	"RAL_WEBGPU_BROWSER_OP_BEGIN_ENCODER"
	"RAL_WEBGPU_BROWSER_OP_RECORD_INDEXED_DRAW"
	"RAL_WEBGPU_BROWSER_OP_SUBMIT"
	"RAL_WEBGPU_BROWSER_OP_POLL_SUBMISSION"
	"RalWebGpu_BrowserBridgeBuildCoreInfo"
	"RalWebGpu_BrowserBridgeBuildPresentationInfo"
	"RalWebGpu_BrowserBridgeBuildResourceInfo"
	"RalWebGpu_BrowserBridgeApplyPipelineInfo"
	"RalWebGpu_BrowserBridgeBuildCommandInfo")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU browser ABI bridge misses required authority: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL WebGPU fixed-width browser ABI policy: PASS")
