# about
## Random Arena wars 
vulkan 1.3 simple cross platform online game.
can load any gltf model, pbr materials, textures, animations and non-linear animations can be loaded
full support for glsl shaders ( compile with glslc/glslangvalidator for vulkan ).
## build requirements
* c++20 compiler
* vulkan( vulkan 1.3, vkbootstrap and vma_mem_alloc)
* glfw
* glm
* fastgltf
* stb_image
* gamenetworkingsockets

## compiling

clone recursively or run ```git submodule --init --update``` to install the dependancy submodules

### cmake
```cmake -G your_platform -B build_dir``` <br>
your_platform can be omitted or can be "Visual Studio 17 2022" or ninja for simplicty if u have it.

### gcc
``glslc -c shaders/*.frag shaders/*.vert``<br>
``g++ -std=c++20 *.cpp */*.cpp -lglfw -lvulkan -lfastgltf -lGameNetworkingSockets -o bin``

visual studio can be used too for windows compiling.

Vulkan SDK has to be installed on the machine for compiling, GameNetworking sockets too and should have the built libraries put in /lib directory so cmake will link them. GameNetworkingSockets will require openssl and protobuf, on windows the dlls are going to be required on path or next to the exe. You can find the pre built dlls in the windows release archive.

## old demo
https://github.com/rarepng/engine/assets/153374928/3d27590c-4bc7-42e4-b4b2-26ca9753ddff
https://github.com/rarepng/engine/assets/153374928/d85023e9-e746-4230-af61-36fb7b283cc4

## new demo
https://rarepng.github.io/vidz/0_1.mp4
https://rarepng.github.io/vidz/0_2.mp4
https://rarepng.github.io/vidz/0_3.mp4


