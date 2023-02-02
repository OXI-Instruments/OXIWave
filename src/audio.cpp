#include "WaveEdit.hpp"
#include <SDL.h>
#include <samplerate.h>

extern int playExportPosition;
extern bool playExport;

float playVolume = -12.0;
float playFrequency = 220.0;
float playFrequencySmooth = playFrequency;
bool playModeXY = false;
bool playEnabled = false;
bool morphInterpolate = true;
float morphX = 0.0;
float morphY = 0.0;
float morphZ = 0.0;
float browse = 0.0;
float browseSpeed = 0.0;
int playIndex = 0;
Bank *playingBank;

static float morphXSmooth = morphX;
static float morphYSmooth = morphY;
static float morphZSmooth = morphZ;
static float browseSmooth = browse;
static SDL_AudioDeviceID audioDevice = 0;
static SDL_AudioSpec audioSpec;
static SRC_STATE *audioSrc = NULL;


long srcCallback(void *cb_data, float **data) {
	float gain = powf(10.0, playVolume / 20.0);
	// Generate next samples
	const int inLen = 64;
	static float in[inLen];
	for (int i = 0; i < inLen; i++) {
		if (morphInterpolate) {
			morphXSmooth = eucmodf(morphX, BANK_GRID_DIM1-0.000001);
			morphYSmooth = eucmodf(morphY, BANK_GRID_DIM2-0.000001);
			morphZSmooth = eucmodf(morphZ, BANK_GRID_DIM3-0.000001);
			browseSmooth = eucmodf(browse, BANK_LEN - 0.000001);
		}
		else {
			// Snap X, Y, Z
			morphXSmooth = roundf(morphX);
			morphYSmooth = roundf(morphY);
			morphZSmooth = roundf(morphZ);
			browseSmooth = roundf(browse);
		}

		int index = (playIndex + i) % WAVE_LEN;

		if ((!playExport && !playEnabled) || (playExportPosition < 0)) {
			in[i] = 0;
		}
		else if (playModeXY && !playExport) {
			// Morph XYZ
			int xi = morphXSmooth;
			float xf = morphXSmooth - xi;

			int yi = morphYSmooth;
			float yf = morphYSmooth - yi;

			int zi = morphZSmooth;
			float zf = morphZSmooth - zi;

			// 3D linear interpolate
			int i_x0 = xi;
			int i_x1 = eucmodi(xi + 1, BANK_GRID_DIM1);

			int i_y0 = yi*BANK_GRID_DIM1;
			int i_y1 = eucmodi(yi + 1, BANK_GRID_DIM2) * BANK_GRID_DIM1;

			int i_z0 = zi*BANK_GRID_DIM1*BANK_GRID_DIM2;
			int i_z1 = eucmodi(zi + 1, BANK_GRID_DIM3)*BANK_GRID_DIM1*BANK_GRID_DIM2;

			float v0 = crossf(
				playingBank->waves[i_z0 + i_y0 + i_x0].postSamples[index],
				playingBank->waves[i_z0 + i_y0 + i_x1].postSamples[index],
				xf);
			float v1 = crossf(
				playingBank->waves[i_z0 + i_y1 + i_x0].postSamples[index],
				playingBank->waves[i_z0 + i_y1 + i_x1].postSamples[index],
				xf);
			float z0 = crossf(v0, v1, yf);

			float v2 = crossf(
				playingBank->waves[i_z1 + i_y0 + i_x0].postSamples[index],
				playingBank->waves[i_z1 + i_y0 + i_x1].postSamples[index],
				xf);
			float v3 = crossf(
				playingBank->waves[i_z1 + i_y1 + i_x0].postSamples[index],
				playingBank->waves[i_z1 + i_y1 + i_x1].postSamples[index],
				xf);
			float z1 = crossf(v2, v3, yf);

			in[i] = crossf(z0, z1, zf);
		}
		else {
			// Morph Z
			int zi = browseSmooth;
			float zf = browseSmooth - zi;
			in[i] = crossf(
				playingBank->waves[zi].postSamples[index],
				playingBank->waves[eucmodi(zi + 1, BANK_LEN)].postSamples[index],
				zf);
		}

		in[i] = clampf(in[i] * gain, -1.0, 1.0);
	}

	playIndex += inLen;
	playIndex %= WAVE_LEN;

	*data = in;
	return inLen;
}


void audioCallback(void *userdata, Uint8 *stream, int len) {
	float *out = (float *) stream;
	int outLen = len / sizeof(float);

	if (playExport) {
		double ratio = (double)audioSpec.freq / WAVE_LEN / playFrequencySmooth;
		src_callback_read(audioSrc, ratio, outLen, out);

		if (playExportPosition < 0) {
			playExportPosition++;
			playIndex = 0;
		} else {
			playExportPosition += audioSpec.samples / WAVE_LEN;
			if ((playExportPosition & 7) == 0)
				browse += 1.0;

			if (browse >= 27.0)
				stopPlayExport();
		}
	}
	else {
		if (!playExport) {
			// Apply exponential smoothing to frequency
			const float lambdaFrequency = 0.5;
			playFrequency = clampf(playFrequency, 1.0, 10000.0);
			playFrequencySmooth = powf(playFrequencySmooth, 1.0 - lambdaFrequency) * powf(playFrequency, lambdaFrequency);
		}
		double ratio = (double)audioSpec.freq / WAVE_LEN / playFrequencySmooth;

		src_callback_read(audioSrc, ratio, outLen, out);

		// Modulate Z
		if (playEnabled && !playModeXY && browseSpeed > 0.f) {
			float deltaZ = browseSpeed * outLen / audioSpec.freq;
			deltaZ = clampf(deltaZ, 0.f, 1.f);
			browse += (BANK_LEN-1) * deltaZ;
			if (browse >= (BANK_LEN-1)) {
				browse = fmodf(browse, (BANK_LEN-1));
				browseSmooth = browse;
			}
		}

		//Browse through all waveforms 8x each
		if (playExport) {
			if (playExportPosition < 0) {
				//silence for lead-in
				playExportPosition++;
				playIndex = 0;
			} else {
				playExportPosition += audioSpec.samples / WAVE_LEN;
				if ((playExportPosition & 7) == 0)
					browse += 1.0;

				if (browse >= 27.0)
					stopPlayExport();
			}
		}

	}
}

int audioGetDeviceCount() {
	return SDL_GetNumAudioDevices(0);
}

const char *audioGetDeviceName(int deviceId) {
	return SDL_GetAudioDeviceName(deviceId, 0);
}

void audioClose() {
	if (audioDevice > 0) {
		SDL_CloseAudioDevice(audioDevice);
	}
}

/** if deviceName is -1, the default audio device is chosen */
void audioOpen(int deviceId) {
	audioClose();

	SDL_AudioSpec spec;
	memset(&spec, 0, sizeof(spec));
	spec.freq = SAMPLE_RATE;
	spec.format = AUDIO_F32;
	spec.channels = 1;
	spec.samples = WAVE_LEN*2;
	spec.callback = audioCallback;

	const char *deviceName = deviceId >= 0 ? SDL_GetAudioDeviceName(deviceId, 0) : NULL;
	// TODO Be more tolerant of devices which can't use floats or 1 channel
	audioDevice = SDL_OpenAudioDevice(deviceName, 0, &spec, &audioSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (audioDevice <= 0)
		return;
	SDL_PauseAudioDevice(audioDevice, 0);
}

void audioInit() {
	assert(!audioSrc);
	int err;
	audioSrc = src_callback_new(srcCallback, SRC_SINC_FASTEST, 1, &err, NULL);
	assert(audioSrc);
	audioOpen(-1);
}

void audioDestroy() {
	audioClose();
	src_delete(audioSrc);
}
