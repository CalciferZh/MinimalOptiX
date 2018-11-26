#include "Material.h"


bool Material::hit(Ray& ray, std::vector<Ray>& scatterd) {
  ray.payload.color = color;
  return true;
}


bool LambertianMaterial::hit(Ray& ray, std::vector<Ray>& scatterd) {
  optix::float3 intersection = ray.point_at(ray.payload.parameter);
  int nNewRay = nScatter / ray.payload.age + 1;
  Ray tmp(intersection, optix::float3());
  tmp.payload.age = ray.payload.age + 1;
  tmp.payload.attenuation = ray.payload.attenuation * albedo;
  tmp.payload.attenuation /= nNewRay;
  for (int i = 0; i < nNewRay; ++i) {
    tmp.direction = ray.payload.normal + Utility::randInUnitSphere();
    scatterd.push_back(tmp);
  }
  return false;
}


bool MetalMaterial::hit(Ray& ray, std::vector<Ray>& scatterd) {
  optix::float3 reflected = Utility::reflect(optix::normalize(ray.direction), ray.payload.normal);
  Ray tmp(ray.point_at(ray.payload.parameter), reflected + fuzz * Utility::randInUnitSphere());
  tmp.payload.age = ray.payload.age + 1;
  tmp.payload.attenuation = ray.payload.attenuation * albedo;
  scatterd.push_back(tmp);
  return false;
}

bool DielectricMaterial::hit(Ray& ray, std::vector<Ray>& scattered) {
  optix::float3 outwardNormal;
  optix::float3 reflected = Utility::reflect(ray.direction, ray.payload.normal);
  optix::float3 refracted;
  float cosine;
  float realRefIdx;
  float reflectProb;
  if (optix::dot(ray.direction, ray.payload.normal) > 0) {
    outwardNormal = -ray.payload.normal;
    realRefIdx = refIdx;
    cosine = optix::dot(ray.direction, ray.payload.normal) / optix::length(ray.direction);
    cosine = sqrt(1 - refIdx * refIdx * (1 - cosine * cosine));
  } else {
    outwardNormal = ray.payload.normal;
    realRefIdx = 1.0f / refIdx;
    cosine = -optix::dot(ray.direction, ray.payload.normal) / optix::length(ray.direction);
  }
  if (Utility::refract(ray.direction, outwardNormal, realRefIdx, refracted)) {
    reflectProb = Utility::schlick(cosine, refIdx);
  } else {
    reflectProb = 1;
  }
  Ray tmp;
  tmp.origin = ray.point_at(ray.payload.parameter);
  tmp.payload.age = ray.payload.age + 1;
  tmp.payload.attenuation = ray.payload.attenuation * albedo;
  if (Utility::randReal() < reflectProb) {
    tmp.direction = reflected;
  } else {
    tmp.direction = refracted;
  }
  scattered.push_back(tmp);
  return false;
}

