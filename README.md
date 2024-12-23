# about

 ![cmake linux ci](https://github.com/rarepng/raw/actions/workflows/cmake_lnx.yml/badge.svg)&ensp;&ensp;
 ![cmake windows ci](https://github.com/rarepng/raw/actions/workflows/cmake_win.yml/badge.svg)<br>

 <br>
 <a href="https://discord.gg/kxRUCKAR3T"><img src="https://img.shields.io/badge/join-discord-%23FFFFFFFF?style=flat&logo=discord&labelColor=black" height="22"></a>

## Random Arena wars 
vulkan 1.3 simple cross platform online multiplayer game.
can load any gltf model, pbr materials, textures, alpha blending, fk and ik animations, actions and non-linear animations
full support for glsl shaders ( compile with glslc/glslangvalidator for vulkan ).

## Downloads

[⏬resources](https://drive.google.com/file/d/1ZwYuB17yq-yRpswRISuvSG-_R7j5GKM9/view?usp=sharing)
### latest binaries
[🐧linux](https://github.com/rarepng/raw/releases/download/alpha/rawl.7z)<br><br>
[🪟windows](https://github.com/rarepng/raw/releases/download/alpha/raw.7z)

## Game controls:

left click - move to location<br>
right click - toggle camera rotation with mouse<br>
R - teleporting ability<br>
F - damaging ability<br>
F4 - fullscreen/window<br>
F3 - network settings, only in main menu<br>
ESC - pause in game, exit in main menu<br>
wasdeq - camera controls, for debugging<br><br>
to play online the host has to forward the port if not on the same local network.


## notes:
All builds are x64.
I am no longer maintaing MSVC builds for the time being. it's slow and very buggy especially around mutexes. windows builds are compiled with mingw.
The binary is only linked to the libraries statically for the time being (minimal/no so/dlls).

## build requirements
* c++20 compiler
* vulkan( vulkan 1.3, vkbootstrap and vma_mem_alloc)
* glfw
* glm
* 🌟 fastgltf -> simdJSON
* stb
* GameNetworkingSockets -> OpenSSL and Protobuf -> abseil

## compiling

### requirements
- ninja (not required but highly preferred)
- cmake
- vulkan sdk ( with glslc or 🌟slangc ) | **slang** is standalone and doesn't come with the vulkan sdk but it's much more flexible.
- gcc | mingw (for windows or cross compiling)
- openssl | openssl compiled with mingw -> msys/strawberry perl (windows)
- install/make sure vulkan sdk is installed and make sure VULKAN_SDK is set and glslc is under VULKAN_SDK/Bin/
- install openssl on linux or build OpenSSL and ensure it's compiled and configured with mingw64 with no-shared and make sure OPENSSL_ROOT_DIR is set, msys2 can be used to make it easier, instructions follow if needed.
- make sure resources are in resources directory
- clone recursively```git clone --recursive``` or run either ```git pull --recurse-submodules``` or ```git submodule update --init --recursive``` after cloning to install the dependancy submodules
- ~~if on windows place {GameNetworkingSockets.dll, libcrypto-3-x64.dll,libprotobuf.dll } next to exe~~
- ~~if on linux make sure openssl, protobuf and gamenetworkingsockets are installed and/or their shared libraries are in lib path, you can run `ldconfig` after installation~~
- previous 2 points useless for now because no shared/dynamic builds at the time being only links statically to these libraries.

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


### cmake notes
on rare occasions shaders don't get compiled or copied by cmake, you can compile them manually with glslc and copy them to build/shaders/ <br>
sometimes resources don't get copied too. copy to build/resources/

#### feel free to contact me if u need help

## new demo
### 3 part multiplayer<br>
<video src=https://github.com/user-attachments/assets/6d03127e-c95a-4dc3-931c-8750e5c5f008>https://rarepng.github.io/vidz/0_1.mp4</video>
<video src=https://github.com/user-attachments/assets/c01b2221-fbd6-42c1-a831-639ce9b4352b>https://rarepng.github.io/vidz/0_2.mp4</video>
<video src=https://github.com/user-attachments/assets/de65abb7-434f-45c6-a867-2995c5120fe6>https://rarepng.github.io/vidz/0_3.mp4</video>



## old demo
### outline and screen space shaders
<video src=https://github.com/rarepng/engine/assets/153374928/3d27590c-4bc7-42e4-b4b2-26ca9753ddff></video>
### instancing
<video src=https://github.com/rarepng/engine/assets/153374928/d85023e9-e746-4230-af61-36fb7b283cc4></video>

## todo
- shared library/linking build
- hot reload for shaders
- integrating slang
- attempt hot reload for
