# about

 ![cmake linux ci](https://github.com/rarepng/raw/actions/workflows/cmake_lnx.yml/badge.svg)&ensp;&ensp;
 ![cmake windows ci](https://github.com/rarepng/raw/actions/workflows/cmake_win.yml/badge.svg)<br>

 <br>
 <a href="https://discord.gg/kxRUCKAR3T"><img src="https://img.shields.io/badge/join-discord-%23FFFFFFFF?style=flat&logo=discord&labelColor=black" height="22"></a>

## Rview 2
vulkan 1.4 simple cross platform multiplayer gltf file viewer.
can load any binary gltf (.glb) model, pbr materials, textures, alpha blending, fk and ik animations, actions and non-linear animations
full support for slang shaders.
gltf extensions are wip.

## downloads
needed for default models:
[⏬resources](https://drive.google.com/drive/folders/1g4OmdRBXKQvmDwKlZVwUiklPCo_V-XA0?usp=sharing)

### latest binaries
[Download](https://github.com/rarepng/rview2/releases/latest)

## controls:

left click - move selected model to location<br>
ESC - exit<br>
wasdeq - camera controls, for debugging<br><br>
Debug Menu - allows u to select model to move/scale/rotate/change animation etc.<br>
Drag & Drop - any model can be loaded if u drag and drop it in the window<br>


## notes:
All builds are x64.
windows builds are compiled with mingw.
The binary is only linked to the libraries statically.

## compiling

### requirements
- ninja
- cmake
- vulkan 1.4+ sdk
- Slang compiler
- gcc 16+ or mingw 16+
- if compiling for networked version: OpenSSL (must be gcc ABI compliant) (will explain how to do that exactly on windows in the instructions section)


#### instructions
- clone recursively```git clone --recursive``` or run either ```git pull --recurse-submodules``` or ```git submodule update --init --recursive``` after cloning to install the dependancy submodules
- download resources and extract to resources directory in repo root
- install vulkan sdk (including slang or install slang standalone (recommended))
- install openssl
	- window only:
		- use msys2 (easiest) to build openssl from source
			- cd /path/to/openssl
			- export PATH="/path/to/mingw/bin:$PATH"
			- export OPENSSL_USE_STATIC_LIBS=ON
			- ./Configure --prefix=$PWD/dist no-idea no-mdc2 no-rc5 no-shared mingw64
			- make depend && make && make install
		- set OPENSSL_ROOT_DIR environment variable to /path/to/openssl/dist or wherever u set the install prefix for openssl to
		- compilers must match for ABI compatibility
- configure cmake from repo root, e.g: 
	- `cmake --preset dbg`
- build:
	- `cmake --build build/dbg`
- install:
	- `cmake --install build/dbg`


#### feel free to contact me if u need help
