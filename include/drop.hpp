#pragma once
#include <string>

enum gltftype{
    flatgltf=0,
    staticgltf,
    animgltf,
    biganimgltf
};


class drop{
public:
    drop();
    gltftype deducegltf(std::string_view pth);
};
