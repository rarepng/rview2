# about

 ![cmake linux ci](https://github.com/rarepng/raw/actions/workflows/cmake_lnx.yml/badge.svg)&ensp;&ensp;
 ![cmake windows ci](https://github.com/rarepng/raw/actions/workflows/cmake_win.yml/badge.svg)<br>

 <br>
 <a href="https://discord.gg/kxRUCKAR3T"><img src="https://img.shields.io/badge/join-discord-%23FFFFFFFF?style=flat&logo=discord&labelColor=black" height="22"></a>

## Rview 2
vulkan 1.4 simple cross platform multiplayer gltf file viewer.
can load any gltf model, pbr materials, textures, alpha blending, fk and ik animations, actions and non-linear animations
full support for glsl shaders ( compile with glslc/glslangvalidator for vulkan ). gltf extensions are wip.

## downloads
needed for default models:
[⏬resources](https://drive.google.com/file/d/1ZwYuB17yq-yRpswRISuvSG-_R7j5GKM9/view?usp=sharing)

### latest binaries
wip

## controls:

left click - move default model to location<br>
ESC - exit<br>
wasdeq - camera controls, for debugging<br><br>
Debug Menu - allows u to choose model to move/scale/rotate/change animation etc.<br>
Drag & Drop - any model can be loaded if u drag and drop it in the window<br>


## notes:
All builds are x64.
windows builds are compiled with mingw.
The binary is only linked to the libraries statically.

## build requirements
* c++20 compiler
* vulkan 1.4
* sdl3
* glm
* 🌟 fastgltf -> simdJSON
* stb
* GameNetworkingSockets -> OpenSSL and Protobuf -> abseil

## compiling

### requirements
- ninja (not required but highly preferred)
- cmake
- vulkan sdk ( with glslc or 🌟slangc ) | **slang** comes with the vulkan sdk since version 1.3.296.0.
- gcc | mingw (for windows or cross compiling)
- openssl | openssl compiled with mingw -> msys/strawberry perl (windows)
- install/make sure vulkan sdk is installed and make sure VULKAN_SDK is set and glslc is under VULKAN_SDK/Bin/
- install openssl on linux or build OpenSSL and ensure it's compiled and configured with mingw64 with no-shared and make sure OPENSSL_ROOT_DIR is set, msys2 can be used to make it easier, instructions follow if needed.
- make sure resources are in resources directory
- clone recursively```git clone --recursive``` or run either ```git pull --recurse-submodules``` or ```git submodule update --init --recursive``` after cloning to install the dependancy submodules

## linux
- download resources and extract to resources directory in project root
- install vulkan sdk
- install openssl 
- configure cmake from project source e.g: 
	- cmake -G Ninja -S . -B build -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
- build the project:
	- cmake --build build

	
## windows
- download resources and extract to resources directory in project root
- install vulkan sdk
- use msys2 to build openssl from source
	- cd /path/to/openssl
	- export PATH="/path/to/mingw/bin:$PATH"
	- export OPENSSL_USE_STATIC_LIBS=ON
	- ./Configure --prefix=$PWD/dist no-idea no-mdc2 no-rc5 no-shared mingw64
	- make depend && make && make install
- set OPENSSL_ROOT_DIR environment variable to /path/to/openssl/dist or wherever u set the install prefix for openssl to
- configure cmake from project source e.g: 
	- cmake -G Ninja -S . -B build -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
- compilers have to match, if u build with mingw, then openssl has to be built with mingw too and the other external projects in cmake
- build the project:
	- cmake --build build

#### feel free to contact me if u need help
