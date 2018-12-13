#pragma once

#include <cstdio>
#include <map>
#include <string>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdint>
#include <optix_world.h>
#include <QImage>

#include "structures.h"
#include "utils_host.h"

// Forked from https://github.com/knightcrawler25/Optix-PathTracer with modification

class Scene {
public:
  Scene(const char* fileName);
  std::vector<std::string> meshNames;
  std::vector<DisneyParams> materials;
  std::vector<std::string> textures;
  std::vector<LightParams> lights;
  int width;
  int height;
};
