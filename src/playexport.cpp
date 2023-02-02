#include "WaveEdit.hpp"
int playExportPosition = 0;
bool playExport = false;


static float cached_playVolume;
static float cached_playFrequency;
static float cached_playFrequencySmooth;
static bool cached_playModeXY;
static bool cached_playEnabled;
static bool cached_morphInterpolate;
static float cached_morphX;
static float cached_morphY;
static float cached_morphZ;
static float cached_browse;
static float cached_browseSpeed;
static int cached_playIndex;

void startPlayExport(void) {
	cached_playEnabled = playEnabled;
	cached_playVolume = playVolume;
	cached_playFrequency = playFrequency;
	cached_playFrequencySmooth = playFrequencySmooth;
	cached_playModeXY = playModeXY;
	cached_morphInterpolate = morphInterpolate;
	cached_browse = browse;
	cached_browseSpeed = browseSpeed;
	cached_playIndex = playIndex;

	playEnabled = false;
	playVolume = 0.0;
	playFrequency = SAMPLE_RATE/(float)WAVE_LEN;
	playFrequencySmooth = playFrequency;
	playModeXY = false;
	morphInterpolate = false;
	browse = 0.0;
	browseSpeed = 0.0;
	playIndex = 0;

	playExportPosition = -1;
	playExport = true;
}

void stopPlayExport(void) {
	playExport = false;

	playVolume = cached_playVolume;
	playFrequency = cached_playFrequency;
	playFrequencySmooth = cached_playFrequencySmooth;
	playModeXY = cached_playModeXY;
	morphInterpolate = cached_morphInterpolate;
	browse = cached_browse;
	browseSpeed = cached_browseSpeed;
	playIndex = cached_playIndex;

	playEnabled = cached_playEnabled;
}
