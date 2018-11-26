#pragma once

#include <stdexcept>
#include <memory>
#include <vector>
#include "Ray.h"
#include "Utility.h"


class Material {
public:
  Material() = default;
  ~Material() = default;

  // if true, the ray is terminated and save ray.payload
  virtual bool hit(Ray& ray, std::vector<Ray>& scatterd);

  optix::float3 color = { 1.0f, 0.0f, 0.0f };
};


class LambertianMaterial : public Material {
public:
  LambertianMaterial() = default;
  LambertianMaterial(optix::float3& a) : albedo(a) {};
  ~LambertianMaterial() = default;

  bool hit(Ray& ray, std::vector<Ray>& scatterd);

  optix::float3 albedo = { 0.5, 0.5, 0.5 };
  int nScatter = 0;
};


class MetalMaterial : public Material {
public:
  MetalMaterial() = default;
  MetalMaterial(optix::float3& a, float f = 0) : albedo(a), fuzz(f) {};
  ~MetalMaterial() = default;

  bool hit(Ray& ray, std::vector<Ray>& scatterd);

  optix::float3 albedo = { 0.8f, 0.8f, 0.8f };
  float fuzz = 0;
};


class DielectricMaterial : public Material {
public:
  DielectricMaterial() = default;
  DielectricMaterial(float ri) : refIdx(ri) {};
  ~DielectricMaterial() = default;

  bool hit(Ray& ray, std::vector<Ray>& scattered);

  optix::float3 albedo = { 1.0f, 1.0f, 1.0f };
  // typically air = 1, glass = 1.3 - 1.7, diamond = 2.4
  float refIdx = 1.5;
};

