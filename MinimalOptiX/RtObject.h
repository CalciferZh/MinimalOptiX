#pragma once

#include <memory>
#include <vector>
#include <vector>
#include "Ray.h"
#include "Geometry.h"
#include "Material.h"


class RtObject
{
public:
  RtObject() = default;
  ~RtObject() = default;

  bool hit(Ray& ray, float tMin, float tMax);
  bool getColor(Ray& ray, std::vector<Ray>& scattered);

  // each call of hit will update this to save with object was hit
  int objIdx = -1;
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Material> material;
  // if subObjects is empty, this RtObject is the final real object
  // otherwise, it's a bounding box for sub-objects
  std::vector<std::shared_ptr<RtObject>> subObjects;
};

