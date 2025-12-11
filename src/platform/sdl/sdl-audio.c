/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-audio.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/internal/gba/audio.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/core/blip_buf.h>

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)

mLOG_DEFINE_CATEGORY(SDL_AUDIO, "SDL Audio", "platform.sdl.audio");

static void _mSDLAudioCallback(void* context, Uint8* data, int len);

static Uint8* cryBuffer = NULL;
static Uint32 cryLength = 0;
static Uint32 cryPosition = 0;
static SDL_mutex* cryMutex = NULL;

static int cryChannels = 0;
static int cryBytesPerSample = 0;  // sizeof(int16_t) * channels
static float cryVolume = 0.35f;   // 35% volume recommended

bool mSDLInitAudio(struct mSDLAudio* context, struct mCoreThread* threadContext) {
#if defined(_WIN32) && SDL_VERSION_ATLEAST(2, 0, 8)
    if (!getenv("SDL_AUDIODRIVER")) {
        _putenv_s("SDL_AUDIODRIVER", "directsound");
    }
#endif

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        mLOG(SDL_AUDIO, ERROR, "Could not initialize SDL sound system: %s", SDL_GetError());
        return false;
    }

    context->desiredSpec.freq = context->sampleRate;
    context->desiredSpec.format = AUDIO_S16SYS;
    context->desiredSpec.channels = 2;
    context->desiredSpec.samples = context->samples;
    context->desiredSpec.callback = _mSDLAudioCallback;
    context->desiredSpec.userdata = context;

    //-----------------------------------------
    // NEW: Initialize cry mixing system
    //-----------------------------------------
    cryMutex = SDL_CreateMutex();
    cryBuffer = NULL;
    cryLength = 0;
    cryPosition = 0;
    //-----------------------------------------

#if SDL_VERSION_ATLEAST(2, 0, 0)
    context->deviceId = SDL_OpenAudioDevice(
        0,
        0,
        &context->desiredSpec,
        &context->obtainedSpec,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
    );

    if (context->deviceId == 0) {
#else
    if (SDL_OpenAudio(&context->desiredSpec, &context->obtainedSpec) < 0) {
#endif
        mLOG(SDL_AUDIO, ERROR, "Could not open SDL sound system");
        return false;
    }

    context->core = 0;

    if (threadContext) {
        context->core = threadContext->core;
        context->sync = &threadContext->impl->sync;

#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_PauseAudioDevice(context->deviceId, 0);
#else
        SDL_PauseAudio(0);
#endif
    }

    return true;
}


void mSDLDeinitAudio(struct mSDLAudio* context) {
	UNUSED(context);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
	SDL_CloseAudioDevice(context->deviceId);
#else
	SDL_PauseAudio(1);
	SDL_CloseAudio();
#endif
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void mSDLPauseAudio(struct mSDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
#else
	UNUSED(context);
	SDL_PauseAudio(1);
#endif
}

void mSDLResumeAudio(struct mSDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 0);
#else
	UNUSED(context);
	SDL_PauseAudio(0);
#endif
}

static void _mSDLAudioCallback(void* context, Uint8* data, int len) {
    struct mSDLAudio* audioContext = context;

    if (!context || !audioContext->core) {
        memset(data, 0, len);
        return;
    }

    // ----------------------------
    // 1. Generate emulator audio
    // ----------------------------
    blip_t* left = audioContext->core->getAudioChannel(audioContext->core, 0);
    blip_t* right = audioContext->core->getAudioChannel(audioContext->core, 1);

    double fauxClock = 1;
    if (audioContext->sync && audioContext->sync->fpsTarget > 0) {
        fauxClock = GBAAudioCalculateRatio(1, audioContext->sync->fpsTarget, 1);
        mCoreSyncLockAudio(audioContext->sync);
    }

    blip_set_rates(left,
                   audioContext->core->frequency(audioContext->core),
                   audioContext->obtainedSpec.freq * fauxClock);
    blip_set_rates(right,
                   audioContext->core->frequency(audioContext->core),
                   audioContext->obtainedSpec.freq * fauxClock);

    int frames = len / (sizeof(int16_t) * audioContext->obtainedSpec.channels);
    int available = blip_samples_avail(left);
    if (available > frames) available = frames;

    int16_t* out = (int16_t*) data;

    blip_read_samples(left, out, available, audioContext->obtainedSpec.channels == 2);
    if (audioContext->obtainedSpec.channels == 2) {
        blip_read_samples(right, out + 1, available, 1);
    }

    if (audioContext->sync) {
        mCoreSyncConsumeAudio(audioContext->sync);
    }

    // Zero-fill missing emulator samples
    if (available < frames) {
        memset(out + audioContext->obtainedSpec.channels * available,
               0,
               (frames - available) * audioContext->obtainedSpec.channels * sizeof(int16_t));
    }

    // ----------------------------
	// 2. Mix Pokémon cry audio
	// ----------------------------
	SDL_LockMutex(cryMutex);

	if (cryBuffer && cryPosition < cryLength) {

		Uint32 outChannels = audioContext->obtainedSpec.channels;
		Uint32 bytesPerOutFrame = sizeof(int16_t) * outChannels;

		Uint32 maxCryFrames = (cryLength - cryPosition) / cryBytesPerSample;
		Uint32 mixFrames = (maxCryFrames < (Uint32)frames) ? maxCryFrames : frames;

		int16_t* crySamples = (int16_t*)(cryBuffer + cryPosition);

		for (Uint32 f = 0; f < mixFrames; f++) {

			if (cryChannels == 1) {
				// mono -> stereo
				int16_t s = crySamples[f];

				for (Uint32 c = 0; c < outChannels; c++) {
					Uint32 idx = f * outChannels + c;
					int mix = out[idx] + (int)(s * cryVolume);

					if (mix > 32767) mix = 32767;
					if (mix < -32768) mix = -32768;

					out[idx] = (int16_t)mix;
				}
			}
			else {
				// stereo cry
				for (Uint32 c = 0; c < outChannels; c++) {
					Uint32 idx = f * outChannels + c;
					int mix = out[idx] + (int)(crySamples[f * outChannels + c] * cryVolume);

					if (mix > 32767) mix = 32767;
					if (mix < -32768) mix = -32768;

					out[idx] = (int16_t)mix;
				}
			}
		}

		cryPosition += mixFrames * cryBytesPerSample;

		if (cryPosition >= cryLength) {
			SDL_free(cryBuffer);
			cryBuffer = NULL;
			cryLength = 0;
			cryPosition = 0;
			cryChannels = 0;
			cryBytesPerSample = 0;
		}
	}

	SDL_UnlockMutex(cryMutex);

}

void mSDLPlayAudio(const char* filename) {
    SDL_AudioSpec spec;
    Uint8* buffer;
    Uint32 length;

    if (!SDL_LoadWAV(filename, &spec, &buffer, &length)) {
        return;
    }

    SDL_LockMutex(cryMutex);

    if (cryBuffer)
        SDL_free(cryBuffer);

    cryBuffer = buffer;
    cryLength = length;
    cryPosition = 0;

    // Store WAV format info for callback
    cryChannels = spec.channels;              // 1 = mono, 2 = stereo
    cryBytesPerSample = sizeof(int16_t) * cryChannels;

    SDL_UnlockMutex(cryMutex);
}
