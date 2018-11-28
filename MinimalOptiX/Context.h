#pragma once

#include <vector>
#include <memory>
#include <limits>
#include <vector>
#include "Ray.h"
#include "MissProgram.h"
#include "RtObject.h"
#include "Geometry.h"
#include "Material.h"
#include "Camera.h"
#include "Utility.h"


class Context {
public:
  Context() = default;

  // Utilities
  void setSize(int w, int h);
  void initDisplayBuffer();
  int getHeight();
  int getWidth();

  // Core
  void launch();
  void rayTrace(Ray& ray);

  // Attributes
  int nSupersampling = 64;
  int rayLifeSpan = 128;
  float rayMinIntensity = 0.01;
  float rayMinParameter = 0.01;
  float rayMaxParameter = std::numeric_limits<float>::max();
  optix::float3 absortColor = { 0.f, 0.f, 0.f };

  // Components
  Ray ray;
  Camera camera;
  std::shared_ptr<MissProgram> mp;
  std::vector<std::shared_ptr<RtObject>> world;
  std::vector<std::vector<optix::float3>> displayBuffer;

private:
  // make sure any change of size will invoke initDisplayBuffer
  int height;
  int width;
};

