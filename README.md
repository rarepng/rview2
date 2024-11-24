# about
## Random Arena wars 
vulkan 1.3 simple cross platform online multiplayer game.
can load any gltf model, pbr materials, textures, alpha blending, fk and ik animations, actions and non-linear animations
full support for glsl shaders ( compile with glslc/glslangvalidator for vulkan ).
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
- clone recursively or run ```git submodule --init --update``` to install the dependancy submodules
- if on windows place {GameNetworkingSockets.dll, libcrypto-3-x64.dll,libprotobuf.dll } next to exe
- if on linux make sure openssl, protobuf and gamenetworkingsockets are installed and/or their shared libraries are in lib path, you can run `ldconfig` after installation

### cmake
```cmake -G your_platform -B build_dir``` <br>
`-G your_platform` can be omitted or can be "-G Visual Studio 17 2022" or ninja for simplicty if u have it.

### gcc
``glslc -c shaders/*.frag shaders/*.vert``<br>
``g++ -std=c++20 *.cpp */*.cpp -lglfw -lvulkan -lfastgltf -lGameNetworkingSockets -o bin``

visual studio can be used too for windows compiling.

Vulkan SDK has to be installed on the machine for compiling, GameNetworking sockets too and should have the built libraries put in /lib directory so cmake will link them. GameNetworkingSockets will require openssl and protobuf, on windows the dlls are going to be required on path or next to the exe. You can find the pre built dlls in the windows release archive.

## old demo
<video src=https://github.com/rarepng/engine/assets/153374928/3d27590c-4bc7-42e4-b4b2-26ca9753ddff></video>
<video src=https://github.com/rarepng/engine/assets/153374928/d85023e9-e746-4230-af61-36fb7b283cc4></video>

## new demo
<video src=https://rarepng.github.io/vidz/0_1.mp4 /></video>
<video src=https://rarepng.github.io/vidz/0_2.mp4 /></video>
<video src=https://rarepng.github.io/vidz/0_3.mp4 /></video>


