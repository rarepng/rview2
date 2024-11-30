# about
 ![cmake linux ci](https://github.com/rarepng/raw/actions/workflows/cmake_lnx.yml/badge.svg)
 ![cmake windows ci](https://github.com/rarepng/raw/actions/workflows/cmake_win.yml/badge.svg)<br>
 <br>
 <a href="https://discord.gg/kxRUCKAR3T"><img src="https://img.shields.io/badge/join-discord-%237289DA?style=flat&logo=discord&labelColor=white" height="20"></a>
## Random Arena wars 
vulkan 1.3 simple cross platform online multiplayer game.
can load any gltf model, pbr materials, textures, alpha blending, fk and ik animations, actions and non-linear animations
full support for glsl shaders ( compile with glslc/glslangvalidator for vulkan ).

[Drive to resources](https://drive.google.com/file/d/1ZwYuB17yq-yRpswRISuvSG-_R7j5GKM9/view?usp=sharing)



## Game controls:

left click - move to location<br>
right click - toggle camera rotation with mouse<br>
R - teleporting ability<br>
F - damaging ability<br>
F4 - fullscreen/window<br>
F3 - network settings, only in main menu<br>
ESC - pause in game, exit in main menu<br>
wasdeq - camera controls, for debugging<br>


## build requirements
* c++20 compiler
* vulkan( vulkan 1.3, vkbootstrap and vma_mem_alloc)
* glfw
* glm
* 🌟 fastgltf -> simdJSON
* stb
* GameNetworkingSockets -> OpenSSL and Protobuf

## compiling
- install/make sure vulkan sdk is installed and make sure VULKAN_SDK is set and glslc is under VULKAN_SDK/Bin/
- build GameNetworkingSockets and place the library in lib directory
- make sure resources are in resources directory 
- clone recursively or run ```git submodule --init --update``` to install the dependancy submodules
- if on windows place {GameNetworkingSockets.dll, libcrypto-3-x64.dll,libprotobuf.dll } next to exe
- if on linux make sure openssl, protobuf and gamenetworkingsockets are installed and/or their shared libraries are in lib path, you can run `ldconfig` after installation

### cmake
```cmake -B build``` <br>
```cd build```<br>
```make``` if linux, compile with visual studio if windows or ```ninja``` if installed and passed to cmake ```-G Ninja```<br>
if using make, copy resources folder to build directory manually.<br>
sometimes shaders don't get compiled or copied by cmake, you can compile them manually with glslc and copy them to build/shaders/ <br>
sometimes resources don't get copied too. copy to build/resources/
### gcc
``glslc -c shaders/*.frag shaders/*.vert``<br>
``g++ -std=c++20 *.cpp */*.cpp -lglfw -lvulkan -lfastgltf -lGameNetworkingSockets -o bin``

visual studio can be used too for windows compiling.

Vulkan SDK has to be installed on the machine for compiling, GameNetworking sockets too and should have the built libraries put in /lib directory so cmake will link them. GameNetworkingSockets will require openssl and protobuf, on windows the dlls are going to be required on path or next to the exe. You can find the pre built dlls in the windows release archive.

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
