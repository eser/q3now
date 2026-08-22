# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(HEADER "${ROOT}/code/renderer/ral_metal/ral_metal_core.h")
set(SOURCE "${ROOT}/code/renderer/ral_metal/ral_metal_core.mm")
set(TEST_SOURCE "${ROOT}/tests/ral_metal_core_test.mm")
foreach(path IN ITEMS "${HEADER}" "${SOURCE}" "${TEST_SOURCE}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal core contract file: ${path}")
	endif()
endforeach()
file(READ "${HEADER}" H)
file(READ "${SOURCE}" S)
file(READ "${TEST_SOURCE}" T)

foreach(needle IN ITEMS
	"MTLCreateSystemDefaultDevice"
	"newCommandQueue"
	"newBufferWithLength"
	"newTextureWithDescriptor"
	"newSamplerStateWithDescriptor"
	"blitCommandEncoder"
	"addCompletedHandler"
	"waitUntilCompleted"
	"Ral_CapabilityProfileFromCaps( RAL_BACKEND_METAL"
	"Ral_AllocationReceiptBuild"
	"Ral_SubmissionLifecyclePublish"
	"Ral_TransferComplete"
	"Ral_MemoryFailureLedgerPublish"
	"RAL_MEMORY_RECOVERY_RECREATE_BACKEND")
	string(FIND "${S}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal core lost required native/receipt seam: ${needle}")
	endif()
endforeach()

foreach(forbidden IN ITEMS "MoltenVK" "VkInstance" "VkDevice" "VkBuffer" "VkImage")
	string(FIND "${H}${S}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal core gained forbidden compatibility concept: ${forbidden}")
	endif()
endforeach()

foreach(needle IN ITEMS
	"RAL_BACKEND_METAL"
	"RalMetal_CoreReceiptExact"
	"RalMetal_TextureFormatSupportsFeatures"
	"RalMetal_OffscreenReceiptExact"
	"beforeCore"
	"beforeOffscreen"
	"uploadAllocation.allocationGeneration"
	"readbackAllocation.allocationGeneration"
	"textureAllocation.allocationGeneration"
	"transfer.completionGeneration"
	"lossEvent.backendType = RAL_BACKEND_WEBGPU"
	"loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND")
	string(FIND "${H}${T}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal host lost receipt/mutation coverage: ${needle}")
	endif()
endforeach()

foreach(needle IN ITEMS
	"features == 0u"
	"features & ~RAL_TEXTURE_FORMAT_FEATURE_ALL"
	"case RAL_FORMAT_R16G16B16A16_SFLOAT:"
	"RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR"
	"RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND"
	"return ( available & features ) == features ? qtrue : qfalse;")
	string(FIND "${S}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal typed texture-format contract drifted: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"RAL_FORMAT_R16G16B16A16_SFLOAT, hdrFeatures"
	"RAL_FORMAT_R16G16B16A16_SFLOAT, 0u"
	"RAL_FORMAT_R16G16B16A16_SFLOAT, 1u << 31"
	"hdrFeatures | RAL_TEXTURE_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT"
	"RAL_FORMAT_UNDEFINED, RAL_TEXTURE_FORMAT_FEATURE_SAMPLED")
	string(FIND "${T}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal texture-format mutation coverage drifted: ${needle}")
	endif()
endforeach()

message(STATUS "RAL Metal core policy: PASS")
