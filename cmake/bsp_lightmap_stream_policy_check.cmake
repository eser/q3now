if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

function(read_required relative out)
	set(path "${ROOT}/${relative}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing BSP lightmap streaming source: ${relative}")
	endif()
	file(READ "${path}" content)
	set(${out} "${content}" PARENT_SCOPE)
endfunction()

function(require_text content needle reason)
	string(FIND "${content}" "${needle}" position)
	if(position LESS 0)
		message(FATAL_ERROR "${reason}: ${needle}")
	endif()
endfunction()

function(require_ordered content first second reason)
	string(FIND "${content}" "${first}" first_position)
	string(FIND "${content}" "${second}" second_position)
	if(first_position LESS 0 OR second_position LESS 0 OR
			second_position LESS_EQUAL first_position)
		message(FATAL_ERROR "${reason}")
	endif()
endfunction()

function(function_slice content begin_marker end_marker out)
	string(FIND "${content}" "${begin_marker}" begin_position)
	string(FIND "${content}" "${end_marker}" end_position)
	if(begin_position LESS 0 OR end_position LESS 0 OR
			end_position LESS_EQUAL begin_position)
		message(FATAL_ERROR "cannot isolate ${begin_marker}")
	endif()
	math(EXPR length "${end_position} - ${begin_position}")
	string(SUBSTRING "${content}" ${begin_position} ${length} body)
	set(${out} "${body}" PARENT_SCOPE)
endfunction()

read_required("code/render/ral/backends/vulkan/renderer/tr_common.h" flags)
read_required("code/render/ral/backends/vulkan/renderer/tr_image.c" image_source)
read_required("code/render/ral/backends/vulkan/renderer/tr_map.c" map_source)
read_required("code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" stream_source)

require_text("${flags}" "IMGFLAG_STREAM_ASSET_CHUNK"
	"decoded-page streaming must be explicit")
string(REGEX MATCHALL "IMGFLAG_STREAM_ASSET_CHUNK" stream_sites "${map_source}")
list(LENGTH stream_sites stream_site_count)
if(stream_site_count LESS 4)
	message(FATAL_ERROR
		"world, merged, styled and prop lightmap producers must all stream")
endif()
require_text("${image_source}" "vk_ral_queue_asset_chunk_image"
	"R_CreateImage must route opted-in decoded pages to the scheduler owner")
if(map_source MATCHES "vk_upload_image_data[ \\t\\r\\n]*\\([ \\t\\r\\n]*tr\\.lightmaps")
	message(FATAL_ERROR "merged lightmaps regressed to synchronous tile uploads")
endif()

function_slice("${stream_source}"
	"qboolean vk_ral_queue_asset_chunk_image"
	"static void vk_ral_admit_asset_chunks" queue_body)
require_text("${queue_body}" "RAL_RESIDENCY_CLASS_ASSET_CHUNK"
	"lightmap pages must use the shared asset-chunk class")
require_ordered("${queue_body}"
	"VK_BINDLESS_PUBLICATION_PLACEHOLDER"
	"s_ral_asset_chunk_request_count++"
	"fallback publication must precede queue visibility")

function_slice("${stream_source}"
	"static void vk_ral_admit_asset_chunks"
	"void vk_ral_register_image" admit_body)
require_text("${admit_body}" "Ral_ResidencySelectRequests"
	"admission must use the shared deterministic scheduler")
require_text("${admit_body}" "VK_RAL_ASSET_CHUNK_PAGES_PER_FRAME"
	"page admission must remain bounded")
require_text("${admit_body}" "VK_RAL_ASSET_CHUNK_BYTES_PER_FRAME"
	"byte admission must remain bounded")
require_ordered("${admit_body}"
	"Ral_TextureUploadBegin"
	"RAL_RESIDENCY_IN_FLIGHT"
	"a submitted exact ticket must precede in-flight publication state")
if(admit_body MATCHES "Ral_WaitFence|RAL_TIMEOUT_INFINITE")
	message(FATAL_ERROR "map-page admission must not wait on the CPU")
endif()

function_slice("${stream_source}"
	"void vk_ral_drain_pending_uploads"
	"qboolean vk_ral_residency_mip_test" drain_body)
require_ordered("${drain_body}"
	"Ral_TextureAcquireBatchToGraphics"
	"VK_BINDLESS_PUBLICATION_RESIDENT"
	"graphics acquire must precede real-view publication")
require_text("${stream_source}" "request->image == image"
	"unregister must scrub not-yet-submitted chunk requests")
require_text("${stream_source}" "vk_ral_release_upload_ticket( &p->ticket )"
	"unregister must retire in-flight chunk tickets")

message(STATUS "BSP RAL lightmap streaming source policy: PASS")
