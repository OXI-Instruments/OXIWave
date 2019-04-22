VERSION = 1.0

FLAGS = -Wall -Wextra -Wno-unused-parameter -g -Wno-unused -O3 -march=nocona -ffast-math \
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
	# Windows
	FLAGS += -DARCH_WIN
	LDFLAGS += \
		-Ldep/lib -lmingw32 -lSDL2main -lSDL2 -lsamplerate -lsndfile -ljansson -lcurl \
		-lopengl32 -mwindows
	SOURCES += ext/osdialog/osdialog_win.c
	OBJECTS += info.o
info.o: info.rc
	windres $^ $@
endif


.DEFAULT_GOAL := build
build: SphereEdit

run: SphereEdit
	LD_LIBRARY_PATH=dep/lib ./SphereEdit

debug: SphereEdit
ifeq ($(ARCH),mac)
	lldb ./SphereEdit
else
	gdb -ex 'run' ./SphereEdit
endif


OBJECTS += $(SOURCES:%=build/%.o)


SphereEdit: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -frv $(OBJECTS) SphereEdit dist


.PHONY: dist
dist: SphereEdit
	rm -frv dist
	mkdir -p dist/SphereEdit
	cp -R spheres dist/SphereEdit/"Example Spheres"
	cp LICENSE* dist/SphereEdit
	cp doc/SphereEdit_manual.pdf dist/SphereEdit
	#cp doc/manual.pdf dist/SphereEdit/WaveEdit_manual.pdf
ifeq ($(ARCH),lin)
	cp -R logo*.png fonts catalog dist/SphereEdit
	cp SphereEdit SphereEdit.sh dist/SphereEdit
	cp dep/lib/libSDL2-2.0.so.0 dist/SphereEdit
	cp dep/lib/libsamplerate.so.0 dist/SphereEdit
	cp dep/lib/libsndfile.so.1 dist/SphereEdit
	cp dep/lib/libjansson.so.4 dist/SphereEdit
	cp dep/lib/libcurl.so.4 dist/SphereEdit
else ifeq ($(ARCH),mac)
	mkdir -p dist/SphereEdit/SphereEdit.app/Contents/MacOS
	mkdir -p dist/SphereEdit/SphereEdit.app/Contents/Resources
	cp Info.plist dist/SphereEdit/SphereEdit.app/Contents
	cp SphereEdit dist/SphereEdit/SphereEdit.app/Contents/MacOS
	cp iconset/logo*.png dist/SphereEdit/SphereEdit.app/Contents/Resources
	cp -R logo.icns fonts catalog dist/SphereEdit/SphereEdit.app/Contents/Resources
	# Remap dylibs in executable
	otool -L dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
	cp dep/lib/libSDL2-2.0.0.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libSDL2-2.0.0.dylib @executable_path/libSDL2-2.0.0.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
	cp dep/lib/libsamplerate.0.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libsamplerate.0.dylib @executable_path/libsamplerate.0.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
	cp dep/lib/libsndfile.1.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libsndfile.1.dylib @executable_path/libsndfile.1.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
	cp dep/lib/libjansson.4.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libjansson.4.dylib @executable_path/libjansson.4.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
	cp dep/lib/libcurl.4.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libcurl.4.dylib @executable_path/libcurl.4.dylib dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
	otool -L dist/SphereEdit/SphereEdit.app/Contents/MacOS/SphereEdit
else ifeq ($(ARCH),win)
	cp -R logo*.png fonts catalog dist/SphereEdit
	cp SphereEdit.exe dist/SphereEdit
	cp /mingw32/bin/libgcc_s_dw2-1.dll dist/SphereEdit
	cp /mingw32/bin/libwinpthread-1.dll dist/SphereEdit
	cp /mingw32/bin/libstdc++-6.dll dist/SphereEdit
	cp dep/bin/SDL2.dll dist/SphereEdit
	cp dep/bin/libsamplerate-0.dll dist/SphereEdit
	cp dep/bin/libsndfile-1.dll dist/SphereEdit
	cp dep/bin/libjansson-4.dll dist/SphereEdit
	cp dep/bin/libcurl-4.dll dist/SphereEdit
endif
	cd dist && zip -9 -r SphereEdit-$(VERSION)-$(ARCH).zip SphereEdit


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
