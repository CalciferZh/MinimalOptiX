#pragma once

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdint>
#include <optix_world.h>

#include "structures.h"

// Forked from https://github.com/knightcrawler25/Optix-PathTracer with modification

class Scene {
public:
  Scene(const char* fileName);
  std::vector<std::string> meshNames;
  std::vector<DisneyParams> materials;
  std::vector<LightParams> lights;
  std::map<int, std::string> textureMap;
  int width;
  int height;
};
