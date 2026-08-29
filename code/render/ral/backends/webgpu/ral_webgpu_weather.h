// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_WEATHER_H
#define WIRED_RAL_WEBGPU_WEATHER_H

#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"
#include "render_submission.h"
#include "ral_atmosphere.h"
#include "ral_display_visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_WEATHER_SCHEMA_VERSION	 2u
#define RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY 8192u

typedef struct ralWebGpuWeather_s ralWebGpuWeather_t;

typedef struct {
	uint32_t					  schemaVersion;
	ralBackendType_t			  backendType;
	uint64_t					  frameGeneration;
	uint64_t					  executorGeneration;
	ralAtmosphereWeatherReceipt_t plan;
	ralDisplayVisibilityPlan_t   displayVisibility;
	ralWebGpuCommandReceipt_t	  command;
	ralWebGpuSubmissionReceipt_t  submission;
	ralWebGpuPipelineReceipt_t	  renderPipeline;
	uintptr_t					  renderBindGroupIdentity;
	uintptr_t					  vertexBufferIdentity;
	uintptr_t					  indexBufferIdentity;
	uint32_t					  dispatchCount;
	uint32_t					  drawIndexCount;
	qboolean					  ready;
} ralWebGpuWeatherReceipt_t;

qboolean RalWebGpu_WeatherCreate( ralWebGpuRuntime_t *runtime, ralWebGpuRuntimeReceipt_t *runtimeReceipt,
								  ralWebGpuBrowserBridge_t *bridge, uint64_t generation,
								  ralWebGpuWeather_t **outWeather );
void	 RalWebGpu_WeatherDestroy( ralWebGpuWeather_t *weather, const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
qboolean RalWebGpu_WeatherPlan( const renderSubmissionState_t *frontend, const ralAtmospherePlanReceipt_t *atmosphere,
								ralAtmosphereWeatherReceipt_t *outPlan );
qboolean RalWebGpu_WeatherBegin( ralWebGpuWeather_t *weather, const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
								 const renderSubmissionState_t *frontend, const ralAtmosphereWeatherReceipt_t *plan,
								 const ralDisplayVisibilityPlan_t *displayVisibility,
								 uint64_t frameGeneration, ralWebGpuWeatherReceipt_t *outReceipt );
ralWebGpuAsyncStatus_t RalWebGpu_WeatherPoll( ralWebGpuWeather_t *weather, const ralWebGpuWeatherReceipt_t *receipt );
qboolean RalWebGpu_WeatherReceiptExact( const ralWebGpuWeatherReceipt_t *a, const ralWebGpuWeatherReceipt_t *b );
void	 RalWebGpu_WeatherReset( ralWebGpuWeather_t *weather );
uint32_t RalWebGpu_WeatherLastFailureStage( void );

#ifdef __cplusplus
}
#endif

#endif
