#include "Context.h"


void Context::launch() {
  for (int i = 0; i < width; ++i) {

#ifndef _DEBUG
#pragma omp parallel
#pragma omp for
#endif

    for (int j = 0; j < height; ++j) {
      Ray ray;
      optix::float3 color = { 0.f, 0.f, 0.f };
      for (int k = 0; k < nSupersampling; ++k) {
        ray.payload.reset();
        float u = float(i + Utility::randReal()) / float(width);
        float v = float(j + Utility::randReal()) / float(height);
        camera.getRay(u, v, ray);
        rayTrace(ray);
        color += ray.payload.color;
      }
      displayBuffer[i][j] = color / float(nSupersampling);
    }
  }
}


void Context::rayTrace(Ray& ray) {
  if (optix::length(ray.payload.color) < rayMinIntensity || ray.payload.age > rayLifeSpan) {
    ray.payload.color = absortColor;
    return;
  }
  int nearstObj;
  Payload nearstPayload;
  bool hitAny = false;
  for (int k = 0; k < world.size(); ++k) {
    if (world[k]->hit(ray, rayMinParameter, rayMaxParameter)) {
      if (hitAny == false || nearstPayload.parameter > ray.payload.parameter) {
        nearstPayload = ray.payload;
        nearstObj = k;
        hitAny = true;
      }
    }
  }
  if (hitAny) {
    ray.payload = nearstPayload;
    std::vector<Ray> scattered;
    if (world[nearstObj]->getColor(ray, scattered)) {
      return;
    } else {
      optix::float3 accuColor{ 0.f, 0.f, 0.f };
      for (auto& newRay: scattered) {
        rayTrace(newRay);
        accuColor += newRay.payload.color;
      }
      ray.payload.color = accuColor;
      return;
    }
  } else {
    (*mp)(ray);
  }
}


void Context::initDisplayBuffer() {
  displayBuffer.resize(width);
  for (auto& col : displayBuffer) {
    col.resize(height);
  }
}


void Context::setSize(int w, int h) {
  height = h;
  width = w;
  initDisplayBuffer();
}

int Context::getHeight() {
  return height;
}

int Context::getWidth() {
  return width;
}


