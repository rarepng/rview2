# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-src"
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-build"
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix"
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix/tmp"
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix/src/absl-populate-stamp"
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix/src"
  "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix/src/absl-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix/src/absl-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/silver/qtcan/vulkan/build/x64-Debug/_deps/absl-subbuild/absl-populate-prefix/src/absl-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
