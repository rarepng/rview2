# about
rendering engine,
currently supports opengl 4.6 and vulkan 1.3. todo: dx 12
can load any gltf model, pbr materials, textures, animations and non-linear animations can be loaded
full support for glsl shaders ( compile with glslc for vulkan ).
## requirements
* c++20 compiler
* vulkan( vulkan 1.3, vkbootstrap and vma_mem_alloc)
* glfw
* glm
* tinygltf
* stb_image

## compiling
### g++ for opengl
``g++ -std=c++20 glad.c tiny_gltf.cc *.cpp -lglfw -lOpenGL -o bin``<br>
### g++ for vulkan
``glslc -c shader/*``<br>
``g++ -std=c++20 tiny_gltf.cc *.cpp imgui/*.cpp -lglfw -lvulakn -o bin``

visual studio can be used too for windows compiling (or mingw) by manually importing all the files or cmake by creating the script for it

## samples

example where all shaders are on (except weapon as it's only activated with left click is pressed)

https://github.com/rarepng/engine/assets/153374928/3d27590c-4bc7-42e4-b4b2-26ca9753ddff




https://github.com/rarepng/engine/assets/153374928/d85023e9-e746-4230-af61-36fb7b283cc4





https://github.com/user-attachments/assets/63d6e427-5eb4-4bb6-a07d-62d90d96b0fe

https://github.com/user-attachments/assets/551bdab1-8db9-424e-8017-ecfbf6e85678

https://github.com/user-attachments/assets/21f4966a-4253-433c-8eff-aa960479b978





