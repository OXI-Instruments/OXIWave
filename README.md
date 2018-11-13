# SphereEdit

Modified version of Synthesis Technology WaveEdit, the wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules.

This version works with wavetables that have three dimensions, each of which wraps. Geometrically, this is a hyperdimensional donut (a 3-torus).

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

	./ShpereEdit

You can even try your luck with building the polished distributable. Although this method is unsupported, it may work with some tweaks to the Makefile.

	make dist
