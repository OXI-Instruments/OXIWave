#include "WaveEdit.hpp"

#ifdef ARCH_LIN
	#include <linux/limits.h>
#endif
#ifdef ARCH_MAC
	#include <sys/syslimits.h>
#endif
#include <libgen.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "osdialog/osdialog.h"

#include "lodepng/lodepng.h"

#include "tablabels.hpp"
#include "buttondropdown.hpp"

static bool showTestWindow = false;
static bool showAbout = false;
// static ImTextureID logoTextureLight;
// static ImTextureID logoTextureDark;
// static ImTextureID logoTexture;
char lastFilename[1024] = "";
static int styleId = 0;
int selectedId = 0;
int lastSelectedId = 0;

float morph_step_size = 0.1;

static void refreshStyle();


enum Page {
	EDITOR_PAGE = 0,
	EFFECT_PAGE,
	GRID_PAGE,
	WATERFALL_PAGE,
	IMPORT_PAGE,
	// DB_PAGE,
	NUM_PAGES
};

Page currentPage = EDITOR_PAGE;


static ImVec4 lighten(ImVec4 col, float p) {
	col.x = crossf(col.x, 1.0, p);
	col.y = crossf(col.y, 1.0, p);
	col.z = crossf(col.z, 1.0, p);
	col.w = crossf(col.w, 1.0, p);
	return col;
}

static ImVec4 darken(ImVec4 col, float p) {
	col.x = crossf(col.x, 0.0, p);
	col.y = crossf(col.y, 0.0, p);
	col.z = crossf(col.z, 0.0, p);
	col.w = crossf(col.w, 0.0, p);
	return col;
}

static ImVec4 alpha(ImVec4 col, float a) {
	col.w *= a;
	return col;
}


static ImTextureID loadImage(const char *filename) {
	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	unsigned char *pixels;
	unsigned int width, height;
	unsigned err = lodepng_decode_file(&pixels, &width, &height, filename, LCT_RGBA, 8);
	assert(!err);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (void*)(intptr_t) textureId;
}


static void getImageSize(ImTextureID id, int *width, int *height) {
	GLuint textureId = (GLuint)(intptr_t) id;
	glBindTexture(GL_TEXTURE_2D, textureId);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, height);
	glBindTexture(GL_TEXTURE_2D, 0);
}


static void selectWave(int waveId) {
	selectedId = waveId;
	lastSelectedId = selectedId;
	morphX = (float)(selectedId % BANK_GRID_DIM1);
	morphY = (float)((selectedId / BANK_GRID_DIM1) % BANK_GRID_DIM2);
	morphZ = (float)((selectedId / (BANK_GRID_DIM1 * BANK_GRID_DIM2)) % BANK_GRID_DIM3);
	browse = (float)selectedId;
}


static void refreshMorphSnap() {
	if (!morphInterpolate && browseSpeed <= 0.f) {
		morphX = roundf(morphX);
		morphY = roundf(morphY);
		morphZ = roundf(morphZ);
		browse = roundf(browse);
	}
}

/** Focuses to a page which displays the current bank, useful when loading a new bank and showing the user some visual feedback that the bank has changed. */
static void showCurrentBankPage() {
	switch (currentPage) {
		case EFFECT_PAGE:
		case IMPORT_PAGE:
		// case DB_PAGE:
			currentPage = EDITOR_PAGE;
			break;
		default:
			break;
	}
}

static void menuManual() {
	openBrowser("SphereEdit_manual.pdf");
}

static void menuNewBank() {
	showCurrentBankPage();
	currentBank.clear();
	lastFilename[0] = '\0';
	historyPush();
}

/** Caller must free() return value, guaranteed to not be NULL */
static char *getLastDir() {
	if (lastFilename[0] == '\0') {
		#ifdef ARCH_MAC
			return strdup("../../..");
		#else
			return strdup(".");
		#endif
	}
	else {
		char filename[PATH_MAX];
		strncpy(filename, lastFilename, sizeof(filename));
		char *dir = dirname(filename);
		return strdup(dir);
	}
}

static void menuOpenSphere() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
	if (path) {
		showCurrentBankPage();
		currentBank.loadMultiWAVs(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		historyPush();
		free(path);
	}
	free(dir);
}

static void menuSaveSphereAs() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled Sphere.wav", NULL);
	if (path) {
		currentBank.exportMultiWAVs(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuSaveSphere() {
	if (str_ends_with(lastFilename, ".wav")) 
		currentBank.exportMultiWAVs(lastFilename);
	else
		menuSaveSphereAs();
}

static void menuSaveWaves() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN_DIR, dir, NULL, NULL);
	if (path) {
		currentBank.saveWaves(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuLoadWaves() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN_DIR, dir, NULL, NULL);
	if (path) {
		currentBank.loadWaves(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuQuit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

static void menuSelectAll() {
	selectedId = 0;
	lastSelectedId = BANK_LEN-1;
}

static void menuCopy() {
	currentBank.waves[selectedId].clipboardCopy();
}

static void menuCut() {
	currentBank.waves[selectedId].clipboardCopy();
	currentBank.waves[selectedId].clear();
	historyPush();
}

static void menuPaste() {
	currentBank.waves[selectedId].clipboardPaste();
	historyPush();
}

static void menuClear() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].clear();
	}
	historyPush();
}

static void menuRandomize() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].randomizeEffects();
	}
	historyPush();
}

static void incrementSelectedId(int delta) {
	selectWave(eucmodi(selectedId + delta, BANK_LEN));
}

static int findIDfromMorph(void) {
	int x_contrib = eucmodi((int)roundf(morphX),BANK_GRID_DIM1);
	int y_contrib = eucmodi((int)roundf(morphY),BANK_GRID_DIM2);
	int z_contrib = eucmodi((int)roundf(morphZ),BANK_GRID_DIM3);

	return x_contrib + y_contrib*BANK_GRID_DIM1 + z_contrib*BANK_GRID_DIM1*BANK_GRID_DIM2;
}

static void incrementX(float delta) {
	morphX = eucmodf(morphX + delta, BANK_GRID_DIM1);
	lastSelectedId = selectedId = findIDfromMorph();
}
static void incrementY(float delta) {
	morphY = eucmodf(morphY + delta, BANK_GRID_DIM2);
	lastSelectedId = selectedId = findIDfromMorph();
}
static void incrementZ(float delta) {
	morphZ = eucmodf(morphZ + delta, BANK_GRID_DIM3);
	lastSelectedId = selectedId = findIDfromMorph();
}

static void menuKeyCommands() {
	ImGuiContext &g = *GImGui;
	ImGuiIO &io = ImGui::GetIO();

	if (io.OSXBehaviors ? io.KeySuper : io.KeyCtrl) {
		if (ImGui::IsKeyPressed(SDLK_n) && !io.KeyShift && !io.KeyAlt)
			menuNewBank();
		if (ImGui::IsKeyPressed(SDLK_o) && !io.KeyShift && !io.KeyAlt)
			menuOpenSphere();
		if (ImGui::IsKeyPressed(SDLK_s) && !io.KeyShift && !io.KeyAlt)
			menuSaveSphere();
		if (ImGui::IsKeyPressed(SDLK_s) && io.KeyShift && !io.KeyAlt)
			menuSaveSphereAs();
		if (ImGui::IsKeyPressed(SDLK_q) && !io.KeyShift && !io.KeyAlt)
			menuQuit();
		if (ImGui::IsKeyPressed(SDLK_z) && !io.KeyShift && !io.KeyAlt)
			historyUndo();
		if (ImGui::IsKeyPressed(SDLK_z) && io.KeyShift && !io.KeyAlt)
			historyRedo();
		if (ImGui::IsKeyPressed(SDLK_a) && !io.KeyShift && !io.KeyAlt)
			menuSelectAll();
		if (ImGui::IsKeyPressed(SDLK_c) && !io.KeyShift && !io.KeyAlt)
			menuCopy();
		if (ImGui::IsKeyPressed(SDLK_x) && !io.KeyShift && !io.KeyAlt)
			menuCut();
		if (ImGui::IsKeyPressed(SDLK_v) && !io.KeyShift && !io.KeyAlt)
			menuPaste();
	}
	// I have NO idea why the scancode is needed here but the keycodes are needed for the letters.
	// It looks like SDLZ_F1 is not defined correctly or something.
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F1))
		menuManual();

	if (!io.KeySuper && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
		// Only trigger these key commands if no text box is focused
		if (!g.ActiveId || g.ActiveId != GImGui->InputTextState.Id) {
			if (ImGui::IsKeyPressed(SDLK_r))
				menuRandomize();
			if (ImGui::IsKeyPressed(io.OSXBehaviors ? SDLK_BACKSPACE : SDLK_DELETE))
				menuClear();
			// Pages
			if (ImGui::IsKeyPressed(SDLK_SPACE))
				playEnabled = !playEnabled;
			if (ImGui::IsKeyPressed(SDLK_1))
				currentPage = EDITOR_PAGE;
			if (ImGui::IsKeyPressed(SDLK_2))
				currentPage = EFFECT_PAGE;
			if (ImGui::IsKeyPressed(SDLK_3))
				currentPage = GRID_PAGE;
			if (ImGui::IsKeyPressed(SDLK_4))
				currentPage = WATERFALL_PAGE;
			if (ImGui::IsKeyPressed(SDLK_5))
				currentPage = IMPORT_PAGE;
			// if (ImGui::IsKeyPressed(SDLK_6))
			// 	currentPage = DB_PAGE;

			if (currentPage == GRID_PAGE){
				if (ImGui::IsKeyPressed(SDL_SCANCODE_UP))		incrementY(-1.0*morph_step_size);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_DOWN))		incrementY(morph_step_size);

				if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT))		incrementX(-1.0*morph_step_size);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT))	incrementX(morph_step_size);

				if (ImGui::IsKeyPressed(SDL_SCANCODE_PAGEUP))	incrementZ(-1.0*morph_step_size);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_PAGEDOWN))	incrementZ(morph_step_size);
			}
			else
			{
				if (ImGui::IsKeyPressed(SDL_SCANCODE_UP))
					incrementSelectedId(-3);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_DOWN))
					incrementSelectedId(3);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT))
					incrementSelectedId(-1);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT))
					incrementSelectedId(1);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_PAGEUP))
					incrementSelectedId(-9);
				if (ImGui::IsKeyPressed(SDL_SCANCODE_PAGEDOWN))
					incrementSelectedId(9);
			}
		}
	}
}

void renderWaveMenu() {
	char menuName[128];
	int selectedStart = mini(selectedId, lastSelectedId);
	int selectedEnd = maxi(selectedId, lastSelectedId);
	if (selectedStart != selectedEnd)
		snprintf(menuName, sizeof(menuName), "(Wave %d to %d)", selectedStart, selectedEnd);
	else
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
	ImGui::MenuItem(menuName, NULL, false, false);

	if (ImGui::MenuItem("Clear", "Delete")) {
		menuClear();
	}
	if (ImGui::MenuItem("Randomize Effects", "R")) {
		menuRandomize();
	}

	if (selectedStart != selectedEnd) {
		ImGui::MenuItem("##spacer3", NULL, false, false);
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
		ImGui::MenuItem(menuName, NULL, false, false);
	}

	if (ImGui::MenuItem("Copy", ImGui::GetIO().OSXBehaviors ? "Cmd+C" : "Ctrl+C")) {
		menuCopy();
	}
	if (ImGui::MenuItem("Cut", ImGui::GetIO().OSXBehaviors ? "Cmd+X" : "Ctrl+X")) {
		menuCut();
	}
	if (ImGui::MenuItem("Paste", ImGui::GetIO().OSXBehaviors ? "Cmd+V" : "Ctrl+V", false, clipboardActive)) {
		menuPaste();
	}

	if (ImGui::MenuItem("Open Single Wave...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
		if (path) {
			currentBank.waves[selectedId].loadWAV(path);
			historyPush();
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
	if (ImGui::MenuItem("Save Single Wave As...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
		if (path) {
			currentBank.waves[selectedId].saveWAV(path);
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
}

void renderMenu() {
	menuKeyCommands();

	// Draw main menu
	if (ImGui::BeginMenuBar()) {
		// File
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Sphere", ImGui::GetIO().OSXBehaviors ? "Cmd+N" : "Ctrl+N"))
				menuNewBank();
			if (ImGui::MenuItem("Open Sphere...", ImGui::GetIO().OSXBehaviors ? "Cmd+O" : "Ctrl+O"))
				menuOpenSphere();
			if (ImGui::MenuItem("Save Sphere", ImGui::GetIO().OSXBehaviors ? "Cmd+S" : "Ctrl+S"))
				menuSaveSphere();
			if (ImGui::MenuItem("Save Sphere As...", ImGui::GetIO().OSXBehaviors ? "Cmd+Shift+S" : "Ctrl+Shift+S"))
				menuSaveSphereAs();

			ImGui::MenuItem("##spacer", NULL, false, false);
			if (ImGui::MenuItem("Save Waves to Folder...", NULL))
				menuSaveWaves();
			if (ImGui::MenuItem("Load Waves from Folder...", NULL))
				menuLoadWaves();

			ImGui::MenuItem("##spacer", NULL, false, false);
			if (ImGui::MenuItem("Quit", ImGui::GetIO().OSXBehaviors ? "Cmd+Q" : "Ctrl+Q"))
				menuQuit();

			ImGui::EndMenu();
		}
		// Edit
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", ImGui::GetIO().OSXBehaviors ? "Cmd+Z" : "Ctrl+Z"))
				historyUndo();
			if (ImGui::MenuItem("Redo", ImGui::GetIO().OSXBehaviors ? "Cmd+Shift+Z" : "Ctrl+Shift+Z"))
				historyRedo();
			if (ImGui::MenuItem("Select All", ImGui::GetIO().OSXBehaviors ? "Cmd+A" : "Ctrl+A"))
				menuSelectAll();
			ImGui::MenuItem("##spacer", NULL, false, false);
			renderWaveMenu();
			ImGui::EndMenu();
		}
		// Audio Output
		if (ImGui::BeginMenu("Audio Output")) {
			int deviceCount = audioGetDeviceCount();
			for (int deviceId = 0; deviceId < deviceCount; deviceId++) {
				const char *deviceName = audioGetDeviceName(deviceId);
				if (ImGui::MenuItem(deviceName, NULL, false)) audioOpen(deviceId);
			}
			ImGui::EndMenu();
		}
		// Colors
		if (ImGui::BeginMenu("Colors")) {
			if (ImGui::MenuItem("Black Swan", NULL, styleId == 0)) {
				styleId = 0;
				refreshStyle();
			}
			if (ImGui::MenuItem("White Swan", NULL, styleId == 1)) {
				styleId = 1;
				refreshStyle();
			}
			ImGui::EndMenu();
		}
		// Help
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("About SphereEdit", NULL, false)) 
				showAbout = true;
			if (ImGui::MenuItem("Manual PDF", "F1", false))
				menuManual();
			ImGui::MenuItem("-----------", NULL, false, false);
			ImGui::MenuItem("v" TOSTRING(VERSION), NULL, false, false);
			// if (ImGui::MenuItem("imgui Demo", NULL, showTestWindow)) showTestWindow = !showTestWindow;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}


void renderPreview() {
	ImGui::Checkbox("Play", &playEnabled);

	ImGui::SameLine();
	if (ImGui::Button("Play into SWN"))	startPlayExport();

	ImGui::SameLine();
	ImGui::PushItemWidth(300.0);
	ImGui::SliderFloat("##playVolume", &playVolume, -60.0f, 0.0f, "Volume: %.2f dB");

	ImGui::PushItemWidth(-1.0);
	ImGui::SameLine();
	ImGui::SliderFloat("##playFrequency", &playFrequency, 1.0f, 10000.0f, "Frequency: %.2f Hz", 0.0f);


	ImGui::Checkbox("Morph Interpolate", &morphInterpolate);
	if (playModeXY) {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 4.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::SliderFloat("##Depth", &morphX, 0.0, BANK_GRID_DIM1, "Depth: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Latitude", &morphY, 0.0, BANK_GRID_DIM2, "Latitude: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Longitude", &morphZ, 0.0, BANK_GRID_DIM3, "Longitude: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Keyboard step size", &morph_step_size, 0.001, 1.0, "Keyboard Step Size: %.3f");
	}
	else {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::SliderFloat("##Browse", &browse, 0.0, BANK_LEN - 1, "Browse: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Browse Speed", &browseSpeed, 0.f, 10.f, "Browse Speed: %.3f Hz", 3.f);
	}

	refreshMorphSnap();
}


void renderToolSelector(Tool *tool) {
	if (ImGui::RadioButton("Pencil", *tool == PENCIL_TOOL)) *tool = PENCIL_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Brush", *tool == BRUSH_TOOL)) *tool = BRUSH_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Grab", *tool == GRAB_TOOL)) *tool = GRAB_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Line", *tool == LINE_TOOL)) *tool = LINE_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Eraser", *tool == ERASER_TOOL)) *tool = ERASER_TOOL;
}


void effectSlider(EffectID effect) {
	char id[64];
	snprintf(id, sizeof(id), "##%s", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "%s: %%.3f", effectNames[effect]);
	if (ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text)) {
		currentBank.waves[selectedId].updatePost();
		historyPush();
	}
}


void editorPage() {	
	ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
	{
		float dummyZ = 0.0;
		float dummyX = 0.0;
		ImGui::PushItemWidth(-1);
		renderBankGrid("SidebarGrid", BANK_LEN * 35.0, 1, &dummyZ, &browse);
		refreshMorphSnap();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginGroup();
	{

		Wave *wave = &currentBank.waves[selectedId];
		float *effects = wave->effects;

		static enum Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			currentBank.waves[selectedId].clear();
			historyPush();
		}

		//Category drop-downs
		// ImGui::SameLine();
		// ImGui::Dummy(ImVec2(30.0f,25.0f));
		// ImGui::SameLine();
		ImGui::Text("Load Primitive:");
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

		for (const CatalogCategory &catalogCategory : catalogCategories) {
			ImGui::SameLine();
			ImVec2 sz(80, 25);
			if (ImGui::BeginButtonDropDown(catalogCategory.name.c_str(), sz)) {
				for (const CatalogFile &catalogFile : catalogCategory.files) {
					if (ImGui::Selectable(catalogFile.name.c_str())) {
						memcpy(currentBank.waves[selectedId].samples, catalogFile.samples, sizeof(float) * WAVE_LEN);
						currentBank.waves[selectedId].commitSamples();
						historyPush();
					}
				}
				ImGui::EndButtonDropDown();
			}
		}
		ImGui::PopStyleVar(1);

		ImGui::BeginChild("Editor", ImVec2(0, 0), true);
		{

			ImGui::PushItemWidth(-1);

			// ImGui::SameLine();
			// if (ImGui::RadioButton("Smooth", tool == SMOOTH_TOOL)) tool = SMOOTH_TOOL;

			ImGui::Text("Waveform");
			const int oversample = 4;
			float waveOversample[WAVE_LEN * oversample];
			cyclicOversample(wave->postSamples, waveOversample, WAVE_LEN, oversample);
			if (renderWave("WaveEditor", 200.0, wave->samples, WAVE_LEN, waveOversample, WAVE_LEN * oversample, tool)) {
				currentBank.waves[selectedId].commitSamples();
				historyPush();
			}

			ImGui::Text("Harmonics");
			if (renderHistogram("HarmonicEditor", 200.0, wave->harmonics, WAVE_LEN / 2, wave->postHarmonics, WAVE_LEN / 2, tool)) {
				currentBank.waves[selectedId].commitHarmonics();
				historyPush();
			}

			ImGui::Text("Effects");
			for (int i = 0; i < EFFECTS_LEN; i++) {
				effectSlider((EffectID) i);
			}

			if (ImGui::Checkbox("Cycle", &currentBank.waves[selectedId].cycle)) {
				currentBank.waves[selectedId].updatePost();
				historyPush();
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Normalize", &currentBank.waves[selectedId].normalize)) {
				currentBank.waves[selectedId].updatePost();
				historyPush();
			}
			ImGui::SameLine();
			if (ImGui::Button("Randomize")) {
				currentBank.waves[selectedId].randomizeEffects();
				historyPush();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset")) {
				currentBank.waves[selectedId].clearEffects();
				historyPush();
			}
			ImGui::SameLine();
			if (ImGui::Button("Bake")) {
				currentBank.waves[selectedId].bakeEffects();
				historyPush();
			}

			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}
	ImGui::EndGroup();
}


void effectHistogram(EffectID effect, Tool tool) {
	float value[BANK_LEN];
	float average = 0.0;
	for (int i = 0; i < BANK_LEN; i++) {
		value[i] = currentBank.waves[i].effects[effect];
		average += value[i];
	}
	average /= BANK_LEN;
	float oldAverage = average;

	ImGui::Text("%s", effectNames[effect]);

	char id[64];
	snprintf(id, sizeof(id), "##%sAverage", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "Average %s: %%.3f", effectNames[effect]);
	if (ImGui::SliderFloat(id, &average, 0.0f, 1.0f, text)) {
		// Change the average effect level to the new average
		float deltaAverage = average - oldAverage;
		for (int i = 0; i < BANK_LEN; i++) {
			if (0.0 < average && average < 1.0) {
				currentBank.waves[i].effects[effect] = clampf(currentBank.waves[i].effects[effect] + deltaAverage, 0.0, 1.0);
			}
			else {
				currentBank.waves[i].effects[effect] = average;
			}
			currentBank.waves[i].updatePost();
			historyPush();
		}
	}

	if (renderHistogram(effectNames[effect], 120, value, BANK_LEN, NULL, 0, tool)) {
		for (int i = 0; i < BANK_LEN; i++) {
			if (currentBank.waves[i].effects[effect] != value[i]) {
				// TODO This always selects the highest index. Select the index the mouse is hovering (requires renderHistogram() to return an int)
				selectWave(i);
				currentBank.waves[i].effects[effect] = value[i];
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
	}
}


void effectPage() {
	static Tool tool = PENCIL_TOOL;
	renderToolSelector(&tool);

	ImGui::BeginChild("Effect Editor", ImVec2(0, 0), true); {

		ImGui::PushItemWidth(-1);
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectHistogram((EffectID) i, tool);
		}
		ImGui::PopItemWidth();

		if (ImGui::Button("Cycle All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cycle None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].randomizeEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].clearEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].bakeEffects();
				historyPush();
			}
		}
	}
	ImGui::EndChild();
}

//Todo: Make this more "sphere" like
void gridPage() {
	playModeXY = true;
	ImGui::BeginChild("Grid Page", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		renderBankCube("WaveGrid", &morphX, &morphY, &morphZ);
		refreshMorphSnap();
	}
	ImGui::EndChild();
}

//Todo: Make this more "sphere" like
void waterfallPage() {
	ImGui::BeginChild("3D View", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		static float amplitude = 0.25;
		ImGui::SliderFloat("##amplitude", &amplitude, 0.01, 1.0, "Scale: %.3f", 0.0);
		static float angle = 1.0;
		ImGui::SliderFloat("##angle", &angle, 0.0, 1.0, "Angle: %.3f");

		renderWaterfall("##waterfall", -1.0, amplitude, angle, &browse);
	}
	ImGui::EndChild();
}


void renderMain() {
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y));

	ImGui::Begin("", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_MenuBar);
	{
		// Menu bar
		renderMenu();
		renderPreview();

		// Tab bar
		{
			static const char *tabLabels[NUM_PAGES] = {
				"Waveform Editor",
				"Effect Editor",
				"Grid View",
				"Waterfall View",
				"Import"
//				"WaveEdit Online"
			};
			static int hoveredTab = 0;
			ImGui::TabLabels(NUM_PAGES, tabLabels, (int*)&currentPage, NULL, false, &hoveredTab);
		}


		// Page
		// Reset some audio variables. These might be changed within the pages.
		playModeXY = false;
		playingBank = &currentBank;
		switch (currentPage) {
			case EDITOR_PAGE: editorPage(); break;
			case EFFECT_PAGE: effectPage(); break;
			case GRID_PAGE: gridPage(); break;
			case WATERFALL_PAGE: waterfallPage(); break;
			case IMPORT_PAGE: importPage(); break;
			// case DB_PAGE: dbPage(); break;
			default: break;
		}
	}
	ImGui::End();

	if (showTestWindow) {
		ImGui::ShowTestWindow(&showTestWindow);
		// float col;
		// ImGui::ColorEdit4("My Color", (float*)&col); 
	}

	if (showAbout) {
		showAbout = false;
		ImGui::OpenPopup("About");
	}

   if(ImGui::BeginPopupModal("About", NULL, ImGuiWindowFlags_NoResize))
    // if(ImGui::BeginPopup("About"))
    {
    	ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    	ImFont* font = atlas->Fonts[1];
    	ImGui::PushFont(font);
        ImGui::Text("SphereEdit");
        ImGui::Text("");
		ImGui::PopFont();

        ImGui::Text("SphereEdit is an open-source wavetable editor for the Spherical Wavetable Navigator from 4ms Company. It was developed by Dan Green. Source code can be found at https://github.com/danngreen/SphereEdit");
        ImGui::Text("");

      	ImGui::Text("SphereEdit is based on WaveEdit, a cross-platform (Mac/Windows/Linux) wavetable and bank editor designed for the Synthesis Technology (http://synthtech.com/) E370 Quad Morphing VCO and E352 Cloud Terrarium VCO Eurorack synthesizer modules.");
		ImGui::Text("WaveEdit was developed by Andrew Belt for Synthesis Technology as a stretch goal of the E370 Kickstarter, and is available for free download at http://synthtech.com/waveedit");
        ImGui::Text("");

        ImGui::Text("Much thanks to Andrew Belt and Synthesis Technology!");

        ImGui::Text("");

        if(ImGui::Button("OK"))
        	ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}




static void refreshStyle() {
	const ImVec4 transparent = ImVec4(0.0, 0.0, 0.0, 0.0);

	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.f;
	style.WindowRounding = 6.f;
	style.GrabRounding = 2.f;
	style.ChildWindowRounding = 2.f;
	style.ScrollbarRounding = 2.f;
	style.FrameRounding = 4.f;
	style.ItemSpacing = ImVec2(10.0f, 10.0f);
	style.FramePadding = ImVec2(6.0f, 4.0f);

	if (styleId == 0) {
		style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.48f, 0.49f, 0.37f, 1.00f);
		style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.95f);
		style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_Border]                = ImVec4(0.41f, 0.40f, 0.05f, 1.00f);
		style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.31f, 0.25f, 0.01f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.71f, 0.60f, 0.08f, 1.00f);
		style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
		style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_Button]                = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.31f, 0.25f, 0.01f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.41f, 0.40f, 0.05f, 1.00f);
		style.Colors[ImGuiCol_Header]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_Separator]             = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
		style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.34f, 0.34f, 0.05f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);
		style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
	}
	else if (styleId == 1) {
		style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.46f, 0.46f, 0.46f, 1.00f);
		style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.83f, 0.83f, 0.83f, 0.95f);
		style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
		style.Colors[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_Border]                = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		style.Colors[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(1.00f, 1.00f, 0.88f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(1.00f, 1.00f, 0.88f, 1.00f);
		style.Colors[ImGuiCol_TitleBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.66f, 0.66f, 0.66f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_ComboBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_Button]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.98f, 0.94f, 0.79f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_Header]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_Separator]             = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
		style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.70f, 0.71f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.78f, 0.69f, 0.03f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.91f, 0.86f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);
		style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
	}
/*	else if (styleId == 1) {
		// base16-atelier-dune
		ImVec4 base[16] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x20, 0x20, 0x1d, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x29, 0x28, 0x24, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x6e, 0x6b, 0x5e, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x7d, 0x7a, 0x68, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x99, 0x95, 0x80, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xa6, 0xa2, 0x8c, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe8, 0xe4, 0xcf, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xfe, 0xfb, 0xec, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd7, 0x37, 0x37, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb6, 0x56, 0x11, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xae, 0x95, 0x13, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x60, 0xac, 0x39, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x1f, 0xad, 0x83, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x66, 0x84, 0xe1, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb8, 0x54, 0xd4, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd4, 0x35, 0x52, 0xff)),
		};

		style.Colors[ImGuiCol_Text]                 = base[0x6];
		style.Colors[ImGuiCol_TextDisabled]         = base[0x4];
		style.Colors[ImGuiCol_WindowBg]             = base[0x2];
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x2];
		style.Colors[ImGuiCol_PopupBg]              = base[0x2];
		style.Colors[ImGuiCol_Border]               = transparent;
		style.Colors[ImGuiCol_BorderShadow]         = transparent;
		style.Colors[ImGuiCol_FrameBg]              = base[0x1];
		style.Colors[ImGuiCol_FrameBgHovered]       = base[0x1];
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x1];
		style.Colors[ImGuiCol_TitleBg]              = base[0x3];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x3];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x3];
		style.Colors[ImGuiCol_MenuBarBg]            = base[0x2];
		style.Colors[ImGuiCol_ScrollbarBg]          = base[0x3];
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x4];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x4];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x4];
		style.Colors[ImGuiCol_ComboBg]              = base[0x4];
		style.Colors[ImGuiCol_CheckMark]            = base[0x4];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x4];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x4];
		style.Colors[ImGuiCol_Button]               = base[0x3];
		style.Colors[ImGuiCol_ButtonHovered]        = base[0x4];
		style.Colors[ImGuiCol_ButtonActive]         = base[0x4];
		style.Colors[ImGuiCol_Header]               = base[0x3];
		style.Colors[ImGuiCol_HeaderHovered]        = base[0x3];
		style.Colors[ImGuiCol_HeaderActive]         = base[0x3];
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x2];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x8];
		style.Colors[ImGuiCol_CloseButton]          = base[0x2];
		style.Colors[ImGuiCol_CloseButtonHovered]   = base[0x2];
		style.Colors[ImGuiCol_CloseButtonActive]    = base[0x2];
		style.Colors[ImGuiCol_PlotLines]            = base[0x4];
		style.Colors[ImGuiCol_PlotLinesHovered]     = base[0x4];
		style.Colors[ImGuiCol_PlotHistogram]        = darken(base[0x8], 0.2);
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0x7], 0.2);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x1], 0.5);
	}
	else if (styleId == 2) {
		// base16-atelier-dune
		ImVec4 base[16] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x20, 0x20, 0x1d, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x29, 0x28, 0x24, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x6e, 0x6b, 0x5e, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x7d, 0x7a, 0x68, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x99, 0x95, 0x80, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xa6, 0xa2, 0x8c, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe8, 0xe4, 0xcf, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xfe, 0xfb, 0xec, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd7, 0x37, 0x37, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb6, 0x56, 0x11, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xae, 0x95, 0x13, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x60, 0xac, 0x39, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x1f, 0xad, 0x83, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x66, 0x84, 0xe1, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb8, 0x54, 0xd4, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd4, 0x35, 0x52, 0xff)),
		};

		style.Colors[ImGuiCol_Text]                 = base[0x1];
		style.Colors[ImGuiCol_TextDisabled]         = base[0x2];
		style.Colors[ImGuiCol_WindowBg]             = base[0x7];
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x7];
		style.Colors[ImGuiCol_PopupBg]              = base[0x7];
		style.Colors[ImGuiCol_Border]               = transparent;
		style.Colors[ImGuiCol_BorderShadow]         = transparent;
		style.Colors[ImGuiCol_FrameBg]              = base[0x6];
		style.Colors[ImGuiCol_FrameBgHovered]       = base[0x6];
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x6];
		style.Colors[ImGuiCol_TitleBg]              = base[0x4];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x4];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x4];
		style.Colors[ImGuiCol_MenuBarBg]            = base[0x7];
		style.Colors[ImGuiCol_ScrollbarBg]          = base[0x6];
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x5];
		style.Colors[ImGuiCol_ComboBg]              = base[0x6];
		style.Colors[ImGuiCol_CheckMark]            = base[0x5];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x5];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x5];
		style.Colors[ImGuiCol_Button]               = base[0x5];
		style.Colors[ImGuiCol_ButtonHovered]        = base[0x6];
		style.Colors[ImGuiCol_ButtonActive]         = base[0x6];
		style.Colors[ImGuiCol_Header]               = base[0x6];
		style.Colors[ImGuiCol_HeaderHovered]        = base[0x5];
		style.Colors[ImGuiCol_HeaderActive]         = base[0x5];
		// style.Colors[ImGuiCol_Column]               = base[0x2];
		// style.Colors[ImGuiCol_ColumnHovered]        = base[0x2];
		// style.Colors[ImGuiCol_ColumnActive]         = base[0x2];
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x9];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x9];
		style.Colors[ImGuiCol_CloseButton]          = base[0x2];
		style.Colors[ImGuiCol_CloseButtonHovered]   = base[0x2];
		style.Colors[ImGuiCol_CloseButtonActive]    = base[0x2];
		style.Colors[ImGuiCol_PlotLines]            = base[0x4];
		style.Colors[ImGuiCol_PlotLinesHovered]     = base[0x4];
		style.Colors[ImGuiCol_PlotHistogram]        = base[0xc];
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0x5], 0.8);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x2], 0.5);
		// logoTexture = logoTextureDark;
	}
	else if (styleId == 3) {
		// base16-ashes
		ImVec4 base[16] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x0c, 0x0d, 0x0e, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x2e, 0x2f, 0x30, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x51, 0x52, 0x53, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x73, 0x74, 0x75, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x95, 0x96, 0x97, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb7, 0xb8, 0xb9, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xda, 0xdb, 0xdc, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xfc, 0xfd, 0xfe, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe3, 0x1a, 0x1c, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe6, 0x55, 0x0d, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xdc, 0xa0, 0x60, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x31, 0xa3, 0x54, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x80, 0xb1, 0xd3, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x31, 0x82, 0xbd, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x75, 0x6b, 0xb1, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb1, 0x59, 0x28, 0xff)),
		};

		style.Colors[ImGuiCol_Text]                 = base[0x1];
		style.Colors[ImGuiCol_TextDisabled]         = base[0x2];
		style.Colors[ImGuiCol_WindowBg]             = base[0x7];
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x7];
		style.Colors[ImGuiCol_PopupBg]              = base[0x7];
		style.Colors[ImGuiCol_Border]               = transparent;
		style.Colors[ImGuiCol_BorderShadow]         = transparent;
		style.Colors[ImGuiCol_FrameBg]              = base[0x6];
		style.Colors[ImGuiCol_FrameBgHovered]       = base[0x6];
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x6];
		style.Colors[ImGuiCol_TitleBg]              = base[0x4];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x4];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x4];
		style.Colors[ImGuiCol_MenuBarBg]            = base[0x7];
		style.Colors[ImGuiCol_ScrollbarBg]          = base[0x6];
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x5];
		style.Colors[ImGuiCol_ComboBg]              = base[0x6];
		style.Colors[ImGuiCol_CheckMark]            = base[0x5];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x5];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x5];
		style.Colors[ImGuiCol_Button]               = base[0x5];
		style.Colors[ImGuiCol_ButtonHovered]        = base[0x6];
		style.Colors[ImGuiCol_ButtonActive]         = base[0x6];
		style.Colors[ImGuiCol_Header]               = base[0x6];
		style.Colors[ImGuiCol_HeaderHovered]        = base[0x5];
		style.Colors[ImGuiCol_HeaderActive]         = base[0x5];
		// style.Colors[ImGuiCol_Column]               = base[0x2];
		// style.Colors[ImGuiCol_ColumnHovered]        = base[0x2];
		// style.Colors[ImGuiCol_ColumnActive]         = base[0x2];
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0xd];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0xd];
		style.Colors[ImGuiCol_CloseButton]          = base[0x2];
		style.Colors[ImGuiCol_CloseButtonHovered]   = base[0x2];
		style.Colors[ImGuiCol_CloseButtonActive]    = base[0x2];
		style.Colors[ImGuiCol_PlotLines]            = base[0x4];
		style.Colors[ImGuiCol_PlotLinesHovered]     = base[0x4];
		style.Colors[ImGuiCol_PlotHistogram]        = base[0xd];
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0xc], 0.8);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x2], 0.5);
		// logoTexture = logoTextureDark;
	}
	*/
}


void uiInit() {
	ImGui::GetIO().IniFilename = NULL;
	styleId = 0;

	// Load fonts
	ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 16.0);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 24.0);
	// logoTextureLight = loadImage("logo-light.png");
	// logoTextureDark = loadImage("logo-dark.png");

	// Load UI settings
	// If this gets any more complicated, it should be JSON.
	{
		FILE *f = fopen("ui.dat", "rb");
		if (f) {
			fread(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}

	refreshStyle();
}


void uiDestroy() {
	// Save UI settings
	{
		FILE *f = fopen("ui.dat", "wb");
		if (f) {
			fwrite(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}
}


void uiRender() {
	renderMain();
}
