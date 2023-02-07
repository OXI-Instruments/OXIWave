VERSION = 1.0
FLAGS = -Wall -Wextra -Wno-unused-parameter -g -Wno-unused -O2 -march=nocona -ffast-math \
	-DVERSION=$(VERSION) -DPFFFT_SIMD_DISABLE \
	-I. -Iext -Iext/imgui -Idep/include -Idep/include/SDL2 -I/opt/X11/include 
CFLAGS =
CXXFLAGS = -std=c++11
LDFLAGS =


SOURCES = \
	ext/pffft/pffft.c \
	ext/lodepng/lodepng.cpp \
	ext/imgui/imgui.cpp \
	ext/imgui/imgui_draw.cpp \
	ext/imgui/imgui_demo.cpp \
	ext/imgui/examples/sdl_opengl2_example/imgui_impl_sdl.cpp \
	$(wildcard src/*.cpp)

# OS-specific
include Makefile-arch.inc
ifeq ($(ARCH),lin)
	# Linux
	FLAGS += -DARCH_LIN $(shell pkg-config --cflags gtk+-2.0)
	LDFLAGS += -static-libstdc++ -static-libgcc \
		-lGL -lpthread \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile -ljansson -lcurl \
		-lgtk-x11-2.0 -lgobject-2.0
	SOURCES += ext/osdialog/osdialog_gtk2.c
else ifeq ($(ARCH),mac)
	# Mac
	FLAGS += -DARCH_MAC \
		-mmacosx-version-min=10.7
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -mmacosx-version-min=10.7 \
		-stdlib=libc++ -lpthread \
		-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile -ljansson -lcurl
	SOURCES += ext/osdialog/osdialog_mac.m
else ifeq ($(ARCH),win)

  $(eval GCC_PATH := $(shell where gcc))
  $(eval MINGW_DIR := $(shell dirname $(GCC_PATH)))
  $(eval MINGW_ARCH_DIR := $(shell basename $(shell dirname $(MINGW_DIR))))

	# Windows
	CC = gcc.exe
	FLAGS += -DARCH_WIN -DNDEBUG
	LDFLAGS += \
		-Ldep/lib -lmingw32 -lSDL2main -lSDL2 -lsamplerate -lsndfile -ljansson -lcurl \
		-lopengl32 -mwindows
	SOURCES += ext/osdialog/osdialog_win.c
	OBJECTS += info.o
info.o: info.rc
	windres $^ $@
endif


.DEFAULT_GOAL := build
build: OXIWave

run: OXIWave
	LD_LIBRARY_PATH=dep/lib ./OXIWave

debug: OXIWave
ifeq ($(ARCH),mac)
	lldb ./OXIWave
else
	gdb -ex 'run' ./OXIWave
endif


OBJECTS += $(SOURCES:%=build/%.o)


OXIWave: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:	
	rm -frv $(OBJECTS) OXIWave dist


.PHONY: dist osxdmg
dist: OXIWave
	rm -frv dist
ifeq ($(ARCH),lin)
	mkdir -p dist/OXIWave
	cp -R spheres dist/OXIWave/"Example Spheres"
	cp LICENSE* dist/OXIWave
	cp doc/OXIWave_manual.pdf dist/OXIWave
	cp -R fonts catalog dist/OXIWave
	cp OXIWave OXIWave.sh dist/OXIWave
	cp dep/lib/libSDL2-2.0.so.0 dist/OXIWave
	cp dep/lib/libsamplerate.so.0 dist/OXIWave
	cp dep/lib/libsndfile.so.1 dist/OXIWave
	cp dep/lib/libjansson.so.4 dist/OXIWave
	cp dep/lib/libcurl.so.4 dist/OXIWave
	cd dist && zip -9 -r OXIWave-$(VERSION)-$(ARCH).zip OXIWave
else ifeq ($(ARCH),mac)
	mkdir -p dist/OXIWave
	# cp -R iconset/iconed-folder dist/OXIWave
	cp -R spheres dist/OXIWave/"Example Spheres"
	cp -R Wavetables dist/OXIWave/"CORAL Wavetables"
	cp LICENSE* dist/OXIWave
	cp doc/OXIWave_manual.pdf dist/OXIWave
	mkdir -p dist/OXIWave/OXIWave.app/Contents/MacOS
	mkdir -p dist/OXIWave/OXIWave.app/Contents/Resources
	cp Info.plist dist/OXIWave/OXIWave.app/Contents
	cp OXIWave dist/OXIWave/OXIWave.app/Contents/MacOS
	# img
	# cp iconset/logo*.png dist/OXIWave/OXIWave.app/Contents/Resources
	cp logo-oxi.png dist/OXIWave/OXIWave.app/Contents/Resources
	# logo
	cp -R logo.icns fonts catalog dist/OXIWave/OXIWave.app/Contents/Resources
	# manual
	cp doc/OXIWave_manual.pdf dist/OXIWave/OXIWave.app/Contents/Resources/OXIWave_manual.pdf
	# Remap dylibs in executable
	otool -L dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
	cp dep/lib/libSDL2-2.0.0.dylib dist/OXIWave/OXIWave.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libSDL2-2.0.0.dylib @executable_path/libSDL2-2.0.0.dylib dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
	cp dep/lib/libsamplerate.0.dylib dist/OXIWave/OXIWave.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libsamplerate.0.dylib @executable_path/libsamplerate.0.dylib dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
	cp dep/lib/libsndfile.1.dylib dist/OXIWave/OXIWave.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libsndfile.1.dylib @executable_path/libsndfile.1.dylib dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
	cp dep/lib/libjansson.4.dylib dist/OXIWave/OXIWave.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libjansson.4.dylib @executable_path/libjansson.4.dylib dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
	cp dep/lib/libcurl.4.dylib dist/OXIWave/OXIWave.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libcurl.4.dylib @executable_path/libcurl.4.dylib dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
	otool -L dist/OXIWave/OXIWave.app/Contents/MacOS/OXIWave
else ifeq ($(ARCH),win)	
	mkdir -p dist/OXIWave
	cp -R spheres dist/OXIWave/"Example Spheres"
	cp LICENSE* dist/OXIWave
	cp doc/OXIWave_manual.pdf dist/OXIWave
	cp -R fonts catalog dist/OXIWave
	cp OXIWave.exe dist/OXIWave
	@echo $(MINGW_ARCH_DIR)
    ifeq ($(MINGW_ARCH_DIR),mingw64)
		cp $(MINGW_DIR)/libgcc_s_seh-1.dll dist/OXIWave
    else ifeq ($(MINGW_ARCH_DIR),mingw32)
		cp $(MINGW_DIR)/libgcc_s_dw2-1.dll dist/OXIWave
    endif	
	cp $(MINGW_DIR)/libwinpthread-1.dll dist/OXIWave
	cp $(MINGW_DIR)/libstdc++-6.dll dist/OXIWave
	cp dep/bin/SDL2.dll dist/OXIWave
	cp dep/bin/libsamplerate-0.dll dist/OXIWave
	cp dep/bin/libsndfile-1.dll dist/OXIWave
	cp dep/bin/libjansson-4.dll dist/OXIWave
	cp dep/bin/libcurl-4.dll dist/OXIWave
	cd dist && zip -9 -r OXIWave-$(VERSION)-$(ARCH).zip OXIWave
endif

osxdmg: OXIWave
ifeq ($(ARCH),mac)
	xattr -cr dist/OXIWave/OXIWave.app
	codesign -s "Developer ID Application: OXI Instruments (T3RAH9MKK8)" dist/OXIWave/OXIWave.app/Contents/MacOS/lib*
	codesign -s "Developer ID Application: OXI Instruments (T3RAH9MKK8)" dist/OXIWave/OXIWave.app
	rm -frv OXIWave*.dmg
	rm -frv dist/OXIWave*.dmg
	#Depends on create-dmg: `brew install create-dmg` or see https://github.com/andreyvit/create-dmg
	create-dmg \
		--volname OXIWave-$(VERSION) \
		--volicon iconset/logo.icns \
		--background bkgnd/background-2x.png \
		--window-size 800 680 \
		--icon-size 128 \
		--icon OXIWave 200 370 \
		--app-drop-link 600 370 \
		OXIWave-$(VERSION)-$(ARCH).dmg dist
	mv OXIWave-$(VERSION)-$(ARCH).dmg dist/
	codesign -s "Developer ID Application: OXI" dist/OXIWave-$(VERSION)-$(ARCH).dmg
endif

# SUFFIXES:

build/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<

build/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

build/%.m.o: %.m
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<
