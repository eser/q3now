# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/renderer/ral_metal/ral_metal_bind_group.h")
set(S "${ROOT}/code/renderer/ral_metal/ral_metal_bind_group.mm")
set(T "${ROOT}/tests/ral_metal_bind_group_test.mm")
foreach(path IN ITEMS "${H}" "${S}" "${T}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal bind-group contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)

foreach(needle IN ITEMS
	"newArgumentEncoderWithArguments" "newBufferWithLength" "setArgumentBuffer"
	"setBuffer:" "setTexture:" "setSamplerState:"
	"RAL_BIND_COMBINED_TEXTURE_SAMPLER" "createInfo->bindless != qfalse"
	"source->resourceIdentity != (uintptr_t)source->nativeResource"
	"source->bufferRange > (uint64_t)buffer.length - source->bufferOffset"
	"RalMetal_CoreMatchesReceipt" "RalMetal_BindLayoutReceiptExact" "GroupReceiptValid")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal bind-group lost native/validation seam: ${needle}")
	endif()
endforeach()

foreach(forbidden IN ITEMS "VkDescriptor" "WGPUBindGroup" "MoltenVK")
	string(FIND "${HEADER}${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal bind-group gained forbidden compatibility concept: ${forbidden}")
	endif()
endforeach()

foreach(needle IN ITEMS
	"RAL_BIND_TEXTURE_VIEW_UNSPECIFIED" "RAL_BIND_COMBINED_TEXTURE_SAMPLER"
	"badResources[3].arrayElement = 0u" "badResources[0].bufferRange = 4096u"
	"badResources[0].nativeResource = (void *)sampler" "beforeLayout" "beforeGroup"
	"entries[2].argumentIndex" "resources[0].resourceGeneration")
	string(FIND "${HEADER}${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal bind-group host lost mutation coverage: ${needle}")
	endif()
endforeach()

message(STATUS "RAL Metal bind-group policy: PASS")
