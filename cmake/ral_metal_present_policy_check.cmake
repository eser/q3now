# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/renderer/ral_metal/ral_metal_present.h")
set(S "${ROOT}/code/renderer/ral_metal/ral_metal_present.mm")
set(T "${ROOT}/tests/ral_metal_present_test.mm")
foreach(path IN ITEMS "${H}" "${S}" "${T}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal presentation contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
foreach(needle IN ITEMS
	"CAMetalLayer" "MTLPixelFormatBGRA8Unorm" "MTLPixelFormatRGBA16Float"
	"kCGColorSpaceSRGB" "kCGColorSpaceExtendedLinearDisplayP3"
	"wantsExtendedDynamicRangeContent" "maximumDrawableCount"
	"displaySyncEnabled" "nextDrawable" "presentDrawable"
	"lastAcquireGeneration" "lastPresentGeneration" "CGColorSpaceGetName"
	"RalMetal_CoreBeginCommand" "RalMetal_CorePublishSubmission"
	"RalMetal_PresentAdoptBorrowedLayer" "present->ownsLayer" "receipt.ownsLayer")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation lost native/lifecycle seam: ${needle}")
	endif()
endforeach()
string(FIND "${SOURCE}" "[present->drawable release];\n\tif ( present->ownsLayer ) [present->layer release];" release_order)
if(release_order EQUAL -1)
	message(FATAL_ERROR "Metal presentation lost drawable-before-layer teardown")
endif()
foreach(forbidden IN ITEMS "NSWindow" "SDL_Window" "VkSwapchain" "MoltenVK")
	string(FIND "${HEADER}${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation gained forbidden product/native seam: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"createInfo.desiredWidth = 0u" "unsupportedFormat" "fifo.desiredImageCount = 4u"
	"fifo.unboundedImageCount = 4u" "selected.requestedImageCount"
	"RAL_PRESENT_MAILBOX" "&exactDrawable, sdrClear"
	"staleCore.generation++" "exactLayer.presentationGeneration++"
	"!RalMetal_PresentAcquire" "!RalMetal_PresentReconfigure"
	"sdrClear[0] = NAN" "RAL_FORMAT_R16G16B16A16_SFLOAT"
	"layerReceipt.extendedDynamicRange == qtrue" "RalMetal_CorePublishDeviceLoss"
	"layerReceipt.ownsLayer == qtrue" "exactLayer.ownsLayer = qfalse")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation host lost mutation/SDR-HDR coverage: ${needle}")
	endif()
endforeach()
message(STATUS "RAL Metal presentation policy: PASS")
