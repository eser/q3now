// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_host_lifecycle.h"
#include "wired_profile_imgui.h"
#include "wired_profile_imgui_ral.h"
#include "wired_profile_imgui_sdl3.h"
#include "wired_profile_telemetry_udp.h"

#include "ral.h"
#include "imgui.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "vulkan/vulkan.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct HostContext {
	SDL_Window *window = nullptr;
	ralBackend_t *backend = nullptr;
	ralSwapchain_t *swapchain = nullptr;
	ImGuiContext *imguiContext = nullptr;
	wiredProfileImGuiRalRenderer_t *imguiRenderer = nullptr;
	ralTexture_t *fixtureTexture = nullptr;
	ralTextureView_t *fixtureTextureView = nullptr;
	ralSampler_t *fixtureSampler = nullptr;
	std::uint64_t fixtureTextureId = 0;
	ralProfileSnapshot_t profileSnapshot{};
	double profileHistory[RAL_PROFILE_HISTORY_CAPACITY][RAL_PROFILE_MAX_LANES]{};
	std::uint32_t profileHistoryFrames = 3;
	wiredProfileTelemetryUdp_t *telemetry = nullptr;
	wiredProfileTelemetryUdpReceipt_t telemetryReceipt{};
	bool liveTelemetry = false;
	wiredProfileImGuiSdl3Receipt_t inputReceipt{};
	wiredProfileHostLifecycle_t lifecycle{};
	std::uint64_t acquireCalls = 0;
	std::uint64_t submitCalls = 0;
	std::uint64_t presentCalls = 0;
	std::uint64_t presentedFrames = 0;
	std::uint64_t imguiFrames = 0;
	std::uint64_t liveProfileFrames = 0;
	std::uint64_t countedTelemetrySession = 0;
	std::uint64_t countedTelemetrySequence = 0;
};

void InitProfileFixture( HostContext *host ) {
	ralProfileSnapshot_t &snapshot = host->profileSnapshot;
	snapshot.labels[0] = "wired.main";
	snapshot.labels[1] = "wired.present";
	snapshot.latestMs[0] = 1.25; snapshot.latestMs[1] = 0.50;
	snapshot.averageMs[0] = 1.00; snapshot.averageMs[1] = 0.40;
	snapshot.minimumMs[0] = 0.75; snapshot.minimumMs[1] = 0.25;
	snapshot.maximumMs[0] = 1.50; snapshot.maximumMs[1] = 0.60;
	snapshot.laneCount = 2;
	snapshot.sampleCount = 3;
	snapshot.topologyEpoch = 7;
	host->profileHistory[0][0] = 0.75; host->profileHistory[0][1] = 0.25;
	host->profileHistory[1][0] = 1.00; host->profileHistory[1][1] = 0.45;
	host->profileHistory[2][0] = 1.25; host->profileHistory[2][1] = 0.50;
}

void PollProfileTelemetry( HostContext *host ) {
	if ( !host || !host->telemetry ) return;
	const std::uint64_t before = host->telemetryReceipt.acceptedSequence;
	const bool beforeLive = host->telemetryReceipt.live;
	WiredProfileTelemetryUdp_Poll( host->telemetry, SDL_GetTicksNS() / 1000u,
		&host->telemetryReceipt );
	if ( WiredProfileTelemetryUdp_Snapshot( host->telemetry, &host->profileSnapshot,
		&host->profileHistory[0][0], RAL_PROFILE_HISTORY_CAPACITY,
		&host->telemetryReceipt ) ) {
		host->profileHistoryFrames = host->telemetryReceipt.historyFrames;
	}
	if ( host->telemetryReceipt.acceptedSequence != before ) {
		char labels[512]{};
		size_t used = 0;
		for ( std::uint32_t i = 0; i < host->profileSnapshot.laneCount && used < sizeof( labels ); ++i ) {
			const int written = std::snprintf( labels + used, sizeof( labels ) - used,
				"%s%s", i ? ">" : "", host->profileSnapshot.labels[i] );
			if ( written < 0 || static_cast<size_t>( written ) >= sizeof( labels ) - used ) break;
			used += static_cast<size_t>( written );
		}
		std::fprintf( stdout,
			"RAL_PROFILE_HOST telemetry=accepted session-changes=%llu generation=%u sequence=%llu topology=%u lanes=%u history=%u labels=%s\n",
			static_cast<unsigned long long>( host->telemetryReceipt.sessionChanges ),
			host->telemetryReceipt.producerGeneration,
			static_cast<unsigned long long>( host->telemetryReceipt.acceptedSequence ),
			host->telemetryReceipt.topologyEpoch, host->profileSnapshot.laneCount,
			host->profileHistoryFrames, labels );
	}
	if ( beforeLive && !host->telemetryReceipt.live ) {
		std::fprintf( stdout,
			"RAL_PROFILE_HOST telemetry=inactive status=%u stale-transitions=%llu\n",
			static_cast<unsigned>( host->telemetryReceipt.status ),
			static_cast<unsigned long long>( host->telemetryReceipt.staleTransitions ) );
	}
}

bool CreateFixtureTexture( HostContext *host ) {
	static const std::uint8_t pixels[] = {
		255, 96, 32, 255, 32, 160, 255, 255,
		32, 224, 96, 255, 255, 220, 32, 255
	};
	ralTexture_t *texture = nullptr;
	ralTextureView_t *view = nullptr;
	ralSampler_t *sampler = nullptr;
	ralFence_t *uploadFence = nullptr;
	ralTextureCreateInfo_t tci{};
	ralTextureUploadDesc_t upload{};
	ralTextureViewCreateInfo_t vci{};
	ralSamplerCreateInfo_t sci{};
	std::uint64_t textureId = 0;
	if ( !host || !host->backend || !host->imguiRenderer ) return false;
	if ( host->fixtureTexture && host->fixtureTextureView
	  && host->fixtureSampler && host->fixtureTextureId ) return true;
	if ( host->fixtureTextureId )
		WiredProfileImGuiRal_UnregisterTexture( host->imguiRenderer, host->fixtureTextureId );
	if ( host->fixtureSampler ) Ral_DestroySampler( host->fixtureSampler );
	if ( host->fixtureTextureView ) Ral_DestroyTextureView( host->fixtureTextureView );
	if ( host->fixtureTexture ) Ral_DestroyTexture( host->fixtureTexture );
	host->fixtureTexture = nullptr;
	host->fixtureTextureView = nullptr;
	host->fixtureSampler = nullptr;
	host->fixtureTextureId = 0;
	tci.type = RAL_TEXTURE_2D;
	tci.format = RAL_FORMAT_R8G8B8A8_UNORM;
	tci.width = 2; tci.height = 2; tci.depthOrArrayLayers = 1;
	tci.mipLevels = 1; tci.sampleCount = 1;
	tci.usage = static_cast<ralTextureUsage_t>( RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST );
	tci.memory = RAL_MEMORY_DEVICE_LOCAL;
	tci.debugName = "wired.profile-host.fixture-texture";
	texture = Ral_CreateTexture( host->backend, &tci );
	if ( !texture ) goto fail;
	upload.data = pixels; upload.dataSize = sizeof( pixels ); upload.suppressMipGeneration = qtrue;
	uploadFence = Ral_TextureUploadAsync( texture, &upload );
	if ( !uploadFence ) goto fail;
	Ral_WaitFence( uploadFence, RAL_TIMEOUT_INFINITE );
	Ral_DestroyFence( uploadFence );
	vci.texture = texture; vci.viewType = RAL_TEXTURE_2D;
	vci.format = RAL_FORMAT_R8G8B8A8_UNORM; vci.mipLevelCount = 1; vci.arrayLayerCount = 1;
	view = Ral_CreateTextureView( host->backend, &vci );
	if ( !view ) goto fail;
	sci.minFilter = sci.magFilter = RAL_FILTER_NEAREST;
	sci.mipmapMode = RAL_MIPMAP_NEAREST;
	sci.addressU = sci.addressV = sci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
	sci.maxAnisotropy = 1.0f; sci.compareOp = RAL_COMPARE_ALWAYS; sci.maxLod = 1.0f;
	sci.debugName = "wired.profile-host.fixture-sampler";
	sampler = Ral_CreateSampler( host->backend, &sci );
	if ( !sampler || !WiredProfileImGuiRal_RegisterTexture(
		host->imguiRenderer, view, sampler, &textureId ) ) goto fail;
	host->fixtureTexture = texture;
	host->fixtureTextureView = view;
	host->fixtureSampler = sampler;
	host->fixtureTextureId = textureId;
	return true;

fail:
	if ( textureId ) WiredProfileImGuiRal_UnregisterTexture( host->imguiRenderer, textureId );
	if ( sampler ) Ral_DestroySampler( sampler );
	if ( view ) Ral_DestroyTextureView( view );
	if ( texture ) Ral_DestroyTexture( texture );
	return false;
}

void *HostGetProcAddress( void *, void *nativeInstance, const char *name ) {
	auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
		SDL_Vulkan_GetVkGetInstanceProcAddr() );
	if ( !getInstanceProcAddr || !name ) return nullptr;
	return reinterpret_cast<void *>( getInstanceProcAddr(
		reinterpret_cast<VkInstance>( nativeInstance ), name ) );
}

qboolean HostCreateSurface( void *, void *platformHandle, void *nativeInstance,
	std::uint64_t *outNativeSurface ) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if ( !platformHandle || !nativeInstance || !outNativeSurface ) return qfalse;
	if ( !SDL_Vulkan_CreateSurface( static_cast<SDL_Window *>( platformHandle ),
		reinterpret_cast<VkInstance>( nativeInstance ), nullptr, &surface ) ) return qfalse;
	static_assert( sizeof( surface ) <= sizeof( *outNativeSurface ), "native surface handle too wide" );
	*outNativeSurface = 0;
	std::memcpy( outNativeSurface, &surface, sizeof( surface ) );
	return surface != VK_NULL_HANDLE ? qtrue : qfalse;
}

void HostLog( void *, ralLogSeverity_t severity, const char *message ) {
	std::fprintf( stderr, "RAL_PROFILE_HOST_LOG severity=%d %s",
		static_cast<int>( severity ), message ? message : "(null)\n" );
}

bool GetPixelSize( SDL_Window *window, std::uint32_t *outWidth, std::uint32_t *outHeight ) {
	int width = 0, height = 0;
	if ( !window || !outWidth || !outHeight || !SDL_GetWindowSizeInPixels( window, &width, &height ) )
		return false;
	*outWidth = width > 0 ? static_cast<std::uint32_t>( width ) : 0;
	*outHeight = height > 0 ? static_cast<std::uint32_t>( height ) : 0;
	return true;
}

bool CreateBackend( HostContext *host ) {
	Uint32 extensionCount = 0;
	char const *const *extensions = SDL_Vulkan_GetInstanceExtensions( &extensionCount );
	ralBackendCreateInfo_t ci{};
	if ( !extensions || extensionCount == 0 ) return false;
	ci.type = RAL_BACKEND_VULKAN;
	ci.platformHandle = host->window;
	ci.flags = RAL_FLAG_DEBUG_LABELS;
	ci.host.userData = host;
	ci.host.getProcAddress = HostGetProcAddress;
	ci.host.createSurface = HostCreateSurface;
	ci.host.log = HostLog;
	ci.letBackendOwnInstance = qtrue;
	ci.letBackendOwnDevice = qtrue;
	ci.preferredDeviceIndex = -1;
	ci.enableValidation = qfalse;
	ci.allowAsyncTextureUploads = qfalse;
	ci.platformInstanceExtensions = extensions;
	ci.platformInstanceExtensionCount = extensionCount;
	host->backend = Ral_CreateBackend( &ci );
	return host->backend != nullptr;
}

bool RecreateSwapchain( HostContext *host, std::uint32_t width, std::uint32_t height ) {
	static const ralSurfaceFormat_t formats[] = {
		{ RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR },
		{ RAL_FORMAT_R8G8B8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR }
	};
	static const ralPresentPreference_t preferences[] = {
		{ RAL_PRESENT_FIFO, 3u, 3u }
	};
	ralSwapchainCreateInfo_t ci{};
	ralSwapchainInfo_t oldInfo{}, newInfo{};
	const bool hadOld = host->swapchain && Ral_GetSwapchainInfo( host->swapchain, &oldInfo );
	ci.desiredWidth = width;
	ci.desiredHeight = height;
	ci.formatPreferences = formats;
	ci.formatPreferenceCount = static_cast<std::uint32_t>( sizeof( formats ) / sizeof( formats[0] ) );
	ci.presentPreferences = preferences;
	ci.presentPreferenceCount = 1;
	ci.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	const ralResult_t result = Ral_CreateOrRecreateSwapchain( host->backend, &ci, &host->swapchain );
	if ( result != ralSuccess ) {
		const bool retained = host->swapchain && hadOld;
		WiredProfileHostLifecycle_RecreateFailed( &host->lifecycle, result, retained );
		std::fprintf( stdout,
			"RAL_PROFILE_HOST recreate=result-failed code=%d retained=%d pixels=%ux%u\n",
			static_cast<int>( result ), retained ? 1 : 0, width, height );
		return false;
	}
	if ( !Ral_GetSwapchainInfo( host->swapchain, &newInfo ) || newInfo.generation == 0
	  || newInfo.width == 0 || newInfo.height == 0
	  || !( newInfo.usage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT ) ) return false;
	if ( host->imguiRenderer ) {
		if ( !WiredProfileImGuiRal_EnsureColorFormat( host->imguiRenderer, newInfo.format ) )
			return false;
	} else if ( !host->imguiContext ||
		!WiredProfileImGuiRal_Create( host->backend, newInfo.format,
			host->imguiContext, &host->imguiRenderer ) ) {
		return false;
	}
	if ( !CreateFixtureTexture( host ) ) return false;
	host->lifecycle.pixelWidth = newInfo.width;
	host->lifecycle.pixelHeight = newInfo.height;
	WiredProfileHostLifecycle_RecreateSucceeded( &host->lifecycle, newInfo.generation );
	std::fprintf( stdout,
		"RAL_PROFILE_HOST recreate=ready generation=%llu extent=%ux%u images=%u usage=0x%x\n",
		static_cast<unsigned long long>( newInfo.generation ), newInfo.width, newInfo.height,
		newInfo.imageCount, static_cast<unsigned>( newInfo.usage ) );
	return true;
}

bool RenderClearFrame( HostContext *host, const char *phase ) {
	ralSwapchainInfo_t scInfo{};
	ralSemaphore_t *acquired = nullptr;
	ralSemaphore_t *finished = nullptr;
	ralFence_t *fence = nullptr;
	ralCommandBuffer_t *cb = nullptr;
	ralTexture_t *image = nullptr;
	std::uint32_t imageIndex = UINT32_MAX;
	ralResult_t acquireResult, submitResult, presentResult;
	wiredProfileHostFrameAction_t action;
	wiredProfileImGuiReceipt_t profileReceipt{};
	wiredProfileImGuiRalReceipt_t imguiReceipt{};
	ImDrawData *drawData = nullptr;
	bool recreateAfterPresent = false;
	bool publishFrameReceipt = false;
	bool ok = false;

	if ( !host || host->lifecycle.state != WIRED_PROFILE_HOST_ACTIVE
	  || !host->swapchain || !Ral_GetSwapchainInfo( host->swapchain, &scInfo ) ) return false;
	publishFrameReceipt = !host->liveTelemetry;
	acquired = Ral_CreateSemaphore( host->backend, RAL_SEMAPHORE_BINARY );
	finished = Ral_CreateSemaphore( host->backend, RAL_SEMAPHORE_BINARY );
	fence = Ral_CreateFence( host->backend );
	if ( !acquired || !finished || !fence ) goto cleanup;

	host->acquireCalls++;
	acquireResult = Ral_AcquireNextImage( host->swapchain, RAL_TIMEOUT_INFINITE,
		acquired, &imageIndex, &image );
	action = WiredProfileHostLifecycle_AcquireResult( &host->lifecycle,
		acquireResult, image != nullptr );
	if ( action == WIRED_PROFILE_HOST_FRAME_SKIP ) { ok = true; goto cleanup; }
	if ( action == WIRED_PROFILE_HOST_FRAME_RECREATE ) {
		host->lifecycle.state = WIRED_PROFILE_HOST_RECREATE_PENDING;
		host->lifecycle.recreatePending = true;
		ok = true;
		goto cleanup;
	}
	if ( action != WIRED_PROFILE_HOST_FRAME_RENDER
	  && action != WIRED_PROFILE_HOST_FRAME_RENDER_THEN_RECREATE ) goto cleanup;
	recreateAfterPresent = action == WIRED_PROFILE_HOST_FRAME_RENDER_THEN_RECREATE;
	if ( imageIndex >= scInfo.imageCount || image != Ral_GetSwapchainImage( host->swapchain, imageIndex ) )
		goto cleanup;
	{
		int logicalWidth = 0, logicalHeight = 0;
		if ( !SDL_GetWindowSize( host->window, &logicalWidth, &logicalHeight )
		  || logicalWidth <= 0 || logicalHeight <= 0 ) goto cleanup;
		ImGui::SetCurrentContext( host->imguiContext );
		ImGuiIO &io = ImGui::GetIO();
		io.DisplaySize = ImVec2( static_cast<float>( logicalWidth ),
			static_cast<float>( logicalHeight ) );
		io.DisplayFramebufferScale = ImVec2(
			static_cast<float>( scInfo.width ) / static_cast<float>( logicalWidth ),
			static_cast<float>( scInfo.height ) / static_cast<float>( logicalHeight ) );
		io.DeltaTime = 1.0f / 60.0f;
		ImGui::NewFrame();
		if ( host->liveTelemetry && !host->telemetryReceipt.live ) {
			ImGui::Begin( "RAL Profile" );
			ImGui::TextUnformatted( host->telemetryReceipt.status == R_PROFILE_STATUS_SAMPLING_DISABLED
				? "Live producer connected; r_gpuSpeeds is disabled."
				: "Waiting for a current live renderer sample..." );
			ImGui::End();
		} else if ( !WiredProfileImGui_Draw( &host->profileSnapshot,
			&host->profileHistory[0][0], host->profileHistoryFrames, &profileReceipt ) ) {
			ImGui::EndFrame(); goto cleanup;
		}
		ImGui::Begin( "RAL Texture Fixture" );
		ImGui::TextUnformatted( "font before" );
		ImGui::Image( ImTextureRef( static_cast<ImTextureID>( host->fixtureTextureId ) ), ImVec2( 32.0f, 32.0f ) );
		ImGui::TextUnformatted( "font after" );
		ImGui::End();
		ImGui::Render();
		drawData = ImGui::GetDrawData();
		if ( !drawData || drawData->DisplaySize.x != io.DisplaySize.x
		  || drawData->DisplaySize.y != io.DisplaySize.y ) goto cleanup;
		// Dear ImGui resolves an auto-fit window's final geometry after its
		// first CPU frame. Warm that layout once on the initial generation;
		// the frame accepted below must still contain real indexed draws.
		if ( drawData->TotalVtxCount == 0 || drawData->TotalIdxCount == 0 ) {
			ImGui::NewFrame();
			if ( host->liveTelemetry && !host->telemetryReceipt.live ) {
				ImGui::Begin( "RAL Profile" ); ImGui::TextUnformatted( "Waiting for a current live renderer sample..." ); ImGui::End();
			} else if ( !WiredProfileImGui_Draw( &host->profileSnapshot,
				&host->profileHistory[0][0], host->profileHistoryFrames, &profileReceipt ) ) {
				ImGui::EndFrame(); goto cleanup;
			}
			ImGui::Begin( "RAL Texture Fixture" );
			ImGui::TextUnformatted( "font before" );
			ImGui::Image( ImTextureRef( static_cast<ImTextureID>( host->fixtureTextureId ) ), ImVec2( 32.0f, 32.0f ) );
			ImGui::TextUnformatted( "font after" );
			ImGui::End();
			ImGui::Render();
			drawData = ImGui::GetDrawData();
			if ( !drawData || drawData->TotalVtxCount <= 0 || drawData->TotalIdxCount <= 0 )
				goto cleanup;
		}
	}

	cb = Ral_AcquireBegunCommandBuffer( host->backend, RAL_QUEUE_GRAPHICS );
	if ( !cb ) goto cleanup;
	{
		ralRenderingInfo_t rendering{};
		rendering.colorAttachments[0] = image;
		rendering.colorLoadOps[0] = RAL_LOAD_OP_CLEAR;
		rendering.colorStoreOps[0] = RAL_STORE_OP_STORE;
		rendering.colorClears[0].color[0] = 0.08f;
		rendering.colorClears[0].color[1] = 0.18f;
		rendering.colorClears[0].color[2] = 0.34f;
		rendering.colorClears[0].color[3] = 1.0f;
		rendering.numColorAttachments = 1;
		rendering.renderArea.width = scInfo.width;
		rendering.renderArea.height = scInfo.height;
		rendering.debugName = "wired.profile-host.clear";
		Ral_BeginRendering( cb, &rendering );
		const int imguiRecorded = WiredProfileImGuiRal_Record( host->imguiRenderer,
			drawData, cb, scInfo.width, scInfo.height, &imguiReceipt );
		Ral_EndRendering( cb );
		if ( !imguiRecorded ) goto cleanup;
	}
	if ( Ral_PrepareSwapchainImageForPresent( cb, host->swapchain, imageIndex ) != ralSuccess )
		goto cleanup;
	Ral_EndCommandBuffer( cb );
	{
		ralCommandBuffer_t *commandBuffers[] = { cb };
		ralSemaphore_t *waits[] = { acquired };
		ralSemaphore_t *signals[] = { finished };
		ralSubmitInfo_t submit{};
		submit.commandBuffers = commandBuffers;
		submit.numCommandBuffers = 1;
		submit.waitSemaphores = waits;
		submit.numWaitSemaphores = 1;
		submit.signalSemaphores = signals;
		submit.numSignalSemaphores = 1;
		submit.signalFence = fence;
		host->submitCalls++;
		submitResult = Ral_Submit( host->backend, RAL_QUEUE_GRAPHICS, &submit );
	}
	if ( submitResult != ralSuccess ) goto cleanup;
	{
		ralSwapchain_t *swapchains[] = { host->swapchain };
		ralSemaphore_t *waits[] = { finished };
		ralPresentInfo_t present{};
		present.swapchains = swapchains;
		present.numSwapchains = 1;
		present.imageIndices = &imageIndex;
		present.waitSemaphores = waits;
		present.numWaitSemaphores = 1;
		host->presentCalls++;
		presentResult = Ral_Present( host->backend, &present );
	}
	Ral_WaitFence( fence, RAL_TIMEOUT_INFINITE );
	if ( Ral_WaitQueueIdle( host->backend, RAL_QUEUE_GRAPHICS ) != ralSuccess ) goto cleanup;
	Ral_DrainDeferred( host->backend );
	action = WiredProfileHostLifecycle_PresentResult( &host->lifecycle, presentResult );
	if ( recreateAfterPresent || action == WIRED_PROFILE_HOST_FRAME_RECREATE
	  || action == WIRED_PROFILE_HOST_FRAME_RENDER_THEN_RECREATE ) {
		host->lifecycle.state = WIRED_PROFILE_HOST_RECREATE_PENDING;
		host->lifecycle.recreatePending = true;
	}
	if ( action != WIRED_PROFILE_HOST_FRAME_RENDER
	  && action != WIRED_PROFILE_HOST_FRAME_RECREATE ) goto cleanup;
	if ( presentResult == ralOutOfDate ) { ok = true; goto cleanup; }
	host->presentedFrames++;
	host->imguiFrames++;
	if ( host->liveTelemetry && host->telemetryReceipt.live
	  && ( host->telemetryReceipt.sessionChanges != host->countedTelemetrySession
	    || host->telemetryReceipt.acceptedSequence != host->countedTelemetrySequence ) ) {
		host->liveProfileFrames++;
		host->countedTelemetrySession = host->telemetryReceipt.sessionChanges;
		host->countedTelemetrySequence = host->telemetryReceipt.acceptedSequence;
		publishFrameReceipt = true;
	}
	if ( publishFrameReceipt ) std::fprintf( stdout,
		"RAL_PROFILE_HOST imgui=drawn phase=%s generation=%llu framebuffer=%ux%u lists=%u vertices=%u indices=%u draws=%u scissors=%u texture-binds=%u font-id=%llu font=%ux%u vertex-cap=%llu index-cap=%llu\n",
		phase, static_cast<unsigned long long>( scInfo.generation ), scInfo.width, scInfo.height,
		imguiReceipt.commandLists, imguiReceipt.vertexCount, imguiReceipt.indexCount,
		imguiReceipt.drawCalls, imguiReceipt.scissors, imguiReceipt.textureBinds,
		static_cast<unsigned long long>( imguiReceipt.fontTextureId ),
		imguiReceipt.fontWidth, imguiReceipt.fontHeight,
		static_cast<unsigned long long>( imguiReceipt.vertexCapacityBytes ),
		static_cast<unsigned long long>( imguiReceipt.indexCapacityBytes ) );
	if ( publishFrameReceipt ) std::fprintf( stdout,
		"RAL_PROFILE_HOST frame=presented phase=%s generation=%llu image=%u acquire=%s target=canonical prepare=success submit=success present=%s\n",
		phase, static_cast<unsigned long long>( scInfo.generation ), imageIndex,
		acquireResult == ralSuboptimal ? "suboptimal" : "success",
		presentResult == ralSuboptimal ? "suboptimal" : presentResult == ralOutOfDate ? "out-of-date" : "success" );
	if ( publishFrameReceipt && host->liveTelemetry ) std::fprintf( stdout,
		"RAL_PROFILE_HOST telemetry=presented session-changes=%llu producer-generation=%u sequence=%llu host-generation=%llu lists=%u vertices=%u indices=%u draws=%u present=success\n",
		static_cast<unsigned long long>( host->telemetryReceipt.sessionChanges ),
		host->telemetryReceipt.producerGeneration,
		static_cast<unsigned long long>( host->telemetryReceipt.acceptedSequence ),
		static_cast<unsigned long long>( scInfo.generation ), imguiReceipt.commandLists,
		imguiReceipt.vertexCount, imguiReceipt.indexCount, imguiReceipt.drawCalls );
	ok = true;

cleanup:
	if ( cb ) Ral_DestroyCommandBuffer( cb );
	if ( fence ) Ral_DestroyFence( fence );
	if ( finished ) Ral_DestroySemaphore( finished );
	if ( acquired ) Ral_DestroySemaphore( acquired );
	return ok;
}

bool TickHost( HostContext *host, const char *phase ) {
	if ( !host ) return false;
	PollProfileTelemetry( host );
	if ( host->lifecycle.state == WIRED_PROFILE_HOST_SUSPENDED ) return true;
	if ( host->lifecycle.state == WIRED_PROFILE_HOST_RECREATE_PENDING
	  || ( host->lifecycle.state == WIRED_PROFILE_HOST_ACTIVE
	    && host->lifecycle.recreatePending ) ) {
		if ( host->lifecycle.pixelWidth == 0 || host->lifecycle.pixelHeight == 0 ) return true;
		if ( !RecreateSwapchain( host, host->lifecycle.pixelWidth, host->lifecycle.pixelHeight ) ) {
			if ( host->lifecycle.state == WIRED_PROFILE_HOST_RECREATE_PENDING ) return true;
			if ( host->lifecycle.state != WIRED_PROFILE_HOST_ACTIVE ) return false;
		}
	}
	return host->lifecycle.state == WIRED_PROFILE_HOST_ACTIVE
		&& RenderClearFrame( host, phase );
}

void ProcessEvent( HostContext *host, const SDL_Event &event ) {
	std::uint32_t width = 0, height = 0;
	if ( host->imguiContext ) {
		ImGui::SetCurrentContext( host->imguiContext );
		WiredProfileImGuiSdl3_ProcessEvent( &event, &host->inputReceipt );
	}
	if ( event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ) {
		host->lifecycle.state = WIRED_PROFILE_HOST_QUIT;
		return;
	}
	if ( event.type == SDL_EVENT_WINDOW_MINIMIZED ) {
		WiredProfileHostLifecycle_Minimized( &host->lifecycle );
		return;
	}
	if ( event.type == SDL_EVENT_WINDOW_RESTORED || event.type == SDL_EVENT_WINDOW_SHOWN
	  || event.type == SDL_EVENT_WINDOW_MAXIMIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
	  || event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ) {
		if ( GetPixelSize( host->window, &width, &height ) )
			WiredProfileHostLifecycle_Restored( &host->lifecycle, width, height );
	}
}

bool WaitForWindowEvent( HostContext *host, Uint32 expected, std::uint64_t timeoutMs,
	std::uint32_t *outWidth, std::uint32_t *outHeight ) {
	const Uint64 deadline = SDL_GetTicks() + timeoutMs;
	SDL_Event event;
	while ( SDL_GetTicks() < deadline ) {
		const Sint32 remaining = static_cast<Sint32>( deadline - SDL_GetTicks() );
		if ( !SDL_WaitEventTimeout( &event, remaining > 50 ? 50 : remaining ) ) continue;
		ProcessEvent( host, event );
		if ( event.type == expected ) {
			if ( outWidth && outHeight && !GetPixelSize( host->window, outWidth, outHeight ) ) return false;
			return true;
		}
		if ( host->lifecycle.state == WIRED_PROFILE_HOST_QUIT ) return false;
	}
	return false;
}

void PrintHelp( const char *program ) {
	std::printf( "usage: %s [--frames N | --lifecycle-proof] [--profile-listen PORT --profile-token 32HEX]\n", program );
	std::puts( "  --frames N          present N fixture-backed ImGui/RAL frames" );
	std::puts( "  --lifecycle-proof   real SDL pixel-resize, minimize, restore, and three ImGui-presented generations" );
	std::puts( "  --profile-listen    replace the deterministic profile fixture with loopback-only live telemetry" );
	std::puts( "  --profile-generations N  in live mode, require N producer sessions/generations" );
	std::puts( "  --profile-wait-terminal  in live mode, require the producer-stopped terminal receipt" );
}

} // namespace

int main( int argc, char **argv ) {
	HostContext host;
	bool lifecycleProof = false;
	int frames = 3;
	int rc = 1;
	int profilePort = -1;
	int profileGenerations = 1;
	bool profileWaitTerminal = false;
	const char *profileToken = nullptr;
	std::uint16_t boundProfilePort = 0;
	std::uint32_t width = 0, height = 0, initialWidth = 0, initialHeight = 0;

	for ( int i = 1; i < argc; i++ ) {
		if ( std::strcmp( argv[i], "--help" ) == 0 ) { PrintHelp( argv[0] ); return 0; }
		if ( std::strcmp( argv[i], "--lifecycle-proof" ) == 0 ) { lifecycleProof = true; continue; }
		if ( std::strcmp( argv[i], "--profile-listen" ) == 0 && i + 1 < argc ) {
			profilePort = std::atoi( argv[++i] ); if ( profilePort < 0 || profilePort > 65535 ) return 2; continue;
		}
		if ( std::strcmp( argv[i], "--profile-token" ) == 0 && i + 1 < argc ) { profileToken = argv[++i]; continue; }
		if ( std::strcmp( argv[i], "--profile-generations" ) == 0 && i + 1 < argc ) {
			profileGenerations = std::atoi( argv[++i] );
			if ( profileGenerations < 1 || profileGenerations > 64 ) return 2;
			continue;
		}
		if ( std::strcmp( argv[i], "--profile-wait-terminal" ) == 0 ) { profileWaitTerminal = true; continue; }
		if ( std::strcmp( argv[i], "--frames" ) == 0 && i + 1 < argc ) {
			frames = std::atoi( argv[++i] );
			if ( frames < 1 || frames > 10000 ) { std::fprintf( stderr, "invalid frame count\n" ); return 2; }
			continue;
		}
		PrintHelp( argv[0] );
		return 2;
	}

	WiredProfileHostLifecycle_Init( &host.lifecycle );
	if ( ( profilePort >= 0 ) != ( profileToken != nullptr ) ) { std::fprintf( stderr, "live telemetry requires both port and token\n" ); return 2; }
	if ( profilePort < 0 && ( profileGenerations != 1 || profileWaitTerminal ) ) {
		std::fprintf( stderr, "profile generation/terminal requirements need live telemetry\n" ); return 2;
	}
	if ( profilePort >= 0 ) {
		if ( !WiredProfileTelemetryUdp_Open( static_cast<std::uint16_t>( profilePort ), profileToken,
			&host.telemetry, &boundProfilePort ) ) { std::fprintf( stderr, "live telemetry listener failed\n" ); return 1; }
		host.liveTelemetry = true;
		std::fprintf( stdout, "RAL_PROFILE_HOST telemetry=listening address=127.0.0.1 port=%u\n", boundProfilePort );
	}
	if ( !SDL_Init( SDL_INIT_VIDEO ) ) {
		std::fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
		return 1;
	}
	host.window = SDL_CreateWindow( "Wired RAL Profile Host", 1280, 720,
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
		| SDL_WINDOW_HIDDEN );
	if ( !host.window ) { std::fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() ); goto cleanup; }
	{
		int logicalWidth = 0, logicalHeight = 0;
		int pixelWidth = 0, pixelHeight = 0;
		if ( !SDL_GetWindowSize( host.window, &logicalWidth, &logicalHeight )
			|| !SDL_GetWindowSizeInPixels( host.window, &pixelWidth, &pixelHeight )
			|| logicalWidth != 1280 || logicalHeight != 720
			|| pixelWidth < 1280 || pixelHeight < 720
			|| static_cast<std::int64_t>( pixelWidth ) * 9
				!= static_cast<std::int64_t>( pixelHeight ) * 16 ) {
			std::fprintf( stderr,
				"refusing non-widescreen profile-host window logical=%dx%d pixels=%dx%d\n",
				logicalWidth, logicalHeight, pixelWidth, pixelHeight );
			goto cleanup;
		}
		std::fprintf( stdout,
			"RAL_PROFILE_HOST window-extent logical=%dx%d pixels=%dx%d exact16x9=1 publish-ready=1\n",
			logicalWidth, logicalHeight, pixelWidth, pixelHeight );
	}
	if ( !SDL_ShowWindow( host.window ) ) goto cleanup;
	if ( !CreateBackend( &host ) ) { std::fprintf( stderr, "Ral_CreateBackend failed\n" ); goto cleanup; }
	IMGUI_CHECKVERSION();
	host.imguiContext = ImGui::CreateContext();
	if ( !host.imguiContext ) goto cleanup;
	ImGui::SetCurrentContext( host.imguiContext );
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().ConfigInputTrickleEventQueue = false;
	ImGui::GetIO().Fonts->AddFontDefault();
	if ( !host.liveTelemetry ) InitProfileFixture( &host );
	if ( !SDL_StartTextInput( host.window ) ) goto cleanup;
	if ( !GetPixelSize( host.window, &width, &height ) || width == 0 || height == 0 ) goto cleanup;
	initialWidth = width; initialHeight = height;
	WiredProfileHostLifecycle_Pixels( &host.lifecycle, width, height );
	if ( !TickHost( &host, "initial" ) ) goto cleanup;

	if ( lifecycleProof ) {
		if ( !SDL_SetWindowSize( host.window, 800, 450 ) || !SDL_SyncWindow( host.window ) ) goto cleanup;
		if ( !WaitForWindowEvent( &host, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, 5000, &width, &height ) ) goto cleanup;
		if ( width == initialWidth && height == initialHeight ) goto cleanup;
		std::fprintf( stdout, "RAL_PROFILE_HOST event=pixel-size-changed physical=%ux%u\n", width, height );
		WiredProfileHostLifecycle_Pixels( &host.lifecycle, width, height );
		if ( !TickHost( &host, "resized" ) ) goto cleanup;

		const std::uint64_t acquireBefore = host.acquireCalls;
		const std::uint64_t submitBefore = host.submitCalls;
		const std::uint64_t presentBefore = host.presentCalls;
		const std::uint64_t imguiBefore = host.imguiFrames;
		if ( !SDL_MinimizeWindow( host.window ) || !SDL_SyncWindow( host.window ) ) goto cleanup;
		if ( !WaitForWindowEvent( &host, SDL_EVENT_WINDOW_MINIMIZED, 5000, nullptr, nullptr ) ) goto cleanup;
		if ( !TickHost( &host, "suspended" ) ) goto cleanup;
		SDL_Delay( 250 );
		if ( !TickHost( &host, "suspended" ) ) goto cleanup;
		if ( host.lifecycle.state != WIRED_PROFILE_HOST_SUSPENDED
		  || host.acquireCalls != acquireBefore || host.submitCalls != submitBefore
		  || host.presentCalls != presentBefore || host.imguiFrames != imguiBefore ) goto cleanup;
		std::fprintf( stdout,
			"RAL_PROFILE_HOST event=suspended acquire-delta=0 submit-delta=0 present-delta=0 imgui-delta=0\n" );

		if ( !SDL_RestoreWindow( host.window ) || !SDL_SyncWindow( host.window ) ) goto cleanup;
		if ( !WaitForWindowEvent( &host, SDL_EVENT_WINDOW_RESTORED, 5000, &width, &height ) ) goto cleanup;
		std::fprintf( stdout, "RAL_PROFILE_HOST event=restored physical=%ux%u\n", width, height );
		WiredProfileHostLifecycle_Restored( &host.lifecycle, width, height );
		if ( !TickHost( &host, "restored" ) ) goto cleanup;
	} else {
		const Uint64 deadline = SDL_GetTicks() + 30000;
		while ( ( host.liveTelemetry
			? ( host.liveProfileFrames < static_cast<std::uint64_t>( frames )
			  || host.telemetryReceipt.sessionChanges < static_cast<std::uint64_t>( profileGenerations )
			  || ( profileWaitTerminal && ( host.telemetryReceipt.live
			    || host.telemetryReceipt.status != R_PROFILE_STATUS_RENDERER_STOPPED ) ) )
			: ( host.presentedFrames < static_cast<std::uint64_t>( frames ) ) ) ) {
			SDL_Event event;
			while ( SDL_PollEvent( &event ) ) ProcessEvent( &host, event );
			if ( host.lifecycle.state == WIRED_PROFILE_HOST_QUIT ) { rc = 0; goto cleanup; }
			if ( SDL_GetTicks() >= deadline || !TickHost( &host, "steady" ) ) goto cleanup;
			if ( host.liveTelemetry || host.lifecycle.state != WIRED_PROFILE_HOST_ACTIVE ) SDL_Delay( 10 );
		}
	}

	if ( Ral_WaitQueueIdle( host.backend, RAL_QUEUE_GRAPHICS ) != ralSuccess ) goto cleanup;
	Ral_DrainDeferred( host.backend );
	std::fprintf( stdout,
		"RAL_PROFILE_HOST action=complete acquire=%llu submit=%llu present=%llu\n",
		static_cast<unsigned long long>( host.acquireCalls ),
		static_cast<unsigned long long>( host.submitCalls ),
		static_cast<unsigned long long>( host.presentCalls ) );
	rc = 0;

cleanup:
	if ( host.backend ) Ral_WaitQueueIdle( host.backend, RAL_QUEUE_GRAPHICS );
	if ( host.imguiRenderer && host.fixtureTextureId )
		WiredProfileImGuiRal_UnregisterTexture( host.imguiRenderer, host.fixtureTextureId );
	if ( host.imguiRenderer ) WiredProfileImGuiRal_Destroy( host.imguiRenderer );
	if ( host.imguiContext ) {
		ImGui::SetCurrentContext( host.imguiContext );
		ImGui::DestroyContext( host.imguiContext );
	}
	if ( host.swapchain ) Ral_DestroySwapchain( host.swapchain );
	if ( host.fixtureSampler ) Ral_DestroySampler( host.fixtureSampler );
	if ( host.fixtureTextureView ) Ral_DestroyTextureView( host.fixtureTextureView );
	if ( host.fixtureTexture ) Ral_DestroyTexture( host.fixtureTexture );
	if ( host.backend ) Ral_DestroyBackend( host.backend );
	if ( host.telemetry ) WiredProfileTelemetryUdp_Close( host.telemetry );
	if ( host.window ) { SDL_StopTextInput( host.window ); SDL_DestroyWindow( host.window ); }
	SDL_Quit();
	return rc;
}
