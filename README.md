# OXI Wave

Modified version of Synthesis Technology WaveEdit and 4ms SphereEdit, the wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules.

Original WaveEdit, then adapted to SphereEdit, was written by Andrew Belt and can be found [here](https://github.com/AndrewBelt/WaveEdit/)

OXI Wave allows you to create and edit 3D wavetables suitable for the wavetable engine of our CORAL Eurorack synthesizer.

### Building

Make dependencies with

	cd dep
	make

Clone the in-source dependencies.

	cd ..
	git submodule update --init --recursive

Compile the program. The Makefile will automatically detect your operating system.

	make

Launch the program.

	./OXIWave

You can even try your luck with building the polished distributable. Although this method is unsupported, it may work with some tweaks to the Makefile.

	make dist
