// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* Real, device-less miniaudio engine: no speaker/device, game assets or mocks.
 * The old unbounded 400-unit/s input gives infinite Doppler and millions of
 * input advances per output sample. Inspect that math without running the
 * pathological resampler, then exercise the production bound through PCM mix.
 */
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#include "miniaudio.h"
#include "snd_doppler.h"
#include <float.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int mix_velocity(ma_engine *engine, float x, float y, float z, float expectedSign)
{
	float pcm[480], out[512], v[3] = {x, y, z};
	ma_audio_buffer_ref ref;
	ma_sound sound;
	double energy = 0;
	float c = ma_spatializer_listener_get_speed_of_sound(&engine->listeners[0]);
	wired_audio_bound_doppler_velocity(v, c);
	for (unsigned i = 0; i < 480; ++i) pcm[i] = .1f * sinf(i*.05f);
	CHECK(ma_audio_buffer_ref_init(ma_format_f32, 1, pcm, 480, &ref) == MA_SUCCESS);
	ref.sampleRate = 48000;
	CHECK(ma_sound_init_from_data_source(engine, &ref, 0, NULL, &sound) == MA_SUCCESS);
	ma_sound_set_position(&sound, 0, 0, -100);
	ma_sound_set_velocity(&sound, v[0], v[1], v[2]);
	ma_sound_set_looping(&sound, MA_TRUE);
	CHECK(ma_sound_start(&sound) == MA_SUCCESS);
	for (unsigned i = 0; i < 8; ++i) {
		ma_uint64 read = 0;
		CHECK(ma_engine_read_pcm_frames(engine, out, 256, &read) == MA_SUCCESS);
		CHECK(read == 256);
		float pitch = sound.engineNode.spatializer.dopplerPitch;
		CHECK(isfinite(pitch) && pitch >= .5f && pitch <= MAX_DOPPLER_SCALE + .001f);
		CHECK(sound.engineNode.resampler.state.linear.inAdvanceInt <= (unsigned)MAX_DOPPLER_SCALE);
		if (expectedSign > 0) CHECK(pitch > 1);
		if (expectedSign < 0) CHECK(pitch < 1);
		if (expectedSign == 0) CHECK(pitch == 1);
		for (unsigned j = 0; j < 512; ++j) {
			CHECK(isfinite(out[j]));
			energy += (double)out[j]*out[j];
		}
	}
	CHECK(energy > 0);
	ma_sound_set_doppler_factor(&sound, 0);
	CHECK(ma_engine_read_pcm_frames(engine, out, 256, NULL) == MA_SUCCESS);
	CHECK(sound.engineNode.spatializer.dopplerPitch == 1);
	/* Keep the real ownership barrier; no delayed frees or leaked voices. */
	ma_sound_uninit(&sound);
	ma_audio_buffer_ref_uninit(&ref);
	return 0;
}

int main(void)
{
	ma_engine engine;
	ma_engine_config config = ma_engine_config_init();
	config.noDevice = MA_TRUE; config.channels = 2; config.sampleRate = 48000;
	CHECK(ma_engine_init(&config, &engine) == MA_SUCCESS);
	float c = ma_spatializer_listener_get_speed_of_sound(&engine.listeners[0]);
	ma_vec3f position = {0, 0, 100}, source = {0, 0, 400}, zero = {0, 0, 0};
	CHECK(!isfinite(ma_doppler_pitch(position, source, zero, c, 1)));
	float unchanged[3] = {100, -200, 50};
	wired_audio_bound_doppler_velocity(unchanged, c);
	CHECK(unchanged[0] == 100 && unchanged[1] == -200 && unchanged[2] == 50);
	float a[3] = {400, 600, -800}, b[3] = {-600, -800, -400};
	wired_audio_bound_doppler_velocity(a, c);
	wired_audio_bound_doppler_velocity(b, c);
	CHECK(a[0] == -b[2] && a[1] == -b[0] && a[2] == b[1]);
	const float speeds[] = {0, 300, 343.299f, 343.3f, 400, 900, FLT_MAX, -300, -900, -FLT_MAX};
	for (unsigned i = 0; i < sizeof(speeds)/sizeof(speeds[0]); ++i)
		CHECK(mix_velocity(&engine, 0, 0, speeds[i], speeds[i]) == 0);
	CHECK(mix_velocity(&engine, FLT_MAX, FLT_MAX, FLT_MAX, 1) == 0);
	CHECK(mix_velocity(&engine, NAN, 0, 400, 0) == 0);
	CHECK(mix_velocity(&engine, 0, INFINITY, 400, 0) == 0);
	CHECK(mix_velocity(&engine, 0, 0, -INFINITY, 0) == 0);
	ma_engine_uninit(&engine);
	puts("PASS: bounded Doppler, finite PCM, real loop retirement");
	return 0;
}
