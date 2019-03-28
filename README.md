# SphereEdit

Modified version of Synthesis Technology WaveEdit, the wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules.

Original WaveEdit project, from which SphereEdit is based, was written by Andrew Belt and can be found [here](https://github.com/AndrewBelt/WaveEdit/)

The SphereEdit version works with wavetables that have three dimensions, each of which wraps. Geometrically, this is a hyperdimensional donut (a 3-torus). SphereEdit is made to interface with the Spherical Wavetable Navigator eurorack module from 4ms Company.

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

	./SphereEdit

You can even try your luck with building the polished distributable. Although this method is unsupported, it may work with some tweaks to the Makefile.

	make dist
