# Prerequistes (only needs to be done once)

## Install and configure MSYS2

- Download and install MSYS2 from https://www.msys2.org/
- Open MSYS2 MSYS shell from Start menu (called MSYS2 MSYS, ***not*** the 32/64 bit shells)
- Run `pacman -Sy pacman`
- Close and re-open the same MSYS2 shell
- Run `pacman -Syu`
- Close and re-open the same MSYS2 shell
- Run `pacman -Su`

## Install tools

- Open MSYS2 MSYS shell from Start menu
- Install `make` by running `pacman -S make`
- Install git by running `pacman -S git`
- Install zip by running `pacman -S zip`

## Install the 32-bit compiler

- Open MSYS2 MINGW32 shell from Start menu
- Run `pacman -S mingw-w64-i686-toolchain`

## Install the 64-bit compiler

- Open MSYS2 MINGW64 shell from Start menu
- Run `pacman -S mingw-w64-x86_64-toolchain`

## Install and Update submodules

- Run git submodule update --init --recursive

# Build

## Build for 32-bit

- Open MSYS2 MINGW32 shell from Start menu
- Type c: to navigate to Root /c
- Navigate to `/dep` folder
- Run `make clean ARCH=win`
- Run `make ARCH=win`
- Navigate to `/src` folder
- Run `make clean ARCH=win`
- Run `make ARCH=win`

## Build for 64-bit

- Same as building for 32-bit, but open MSYS2 MINGW64 shell from Start menu instead of MSYS2 MINGW32