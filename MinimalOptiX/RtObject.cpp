#include "RtObject.h"


bool RtObject::hit(Ray& ray, float tMin, float tMax) {
  if (geometry->hit(ray, tMin, tMax)) {
    if (subObjects.empty()) {
      return true;
    } else {
      // traverse sub-objects
    }
  } else {
    return false;
  }
  return false;
}


bool RtObject::getColor(Ray& ray, std::vector<Ray>& scattered) {
  if (subObjects.empty()) {
    if (material) {
      material->hit(ray, scattered);
    } else {
      throw std::logic_error("Mo material attached to RtObject");
    }
  } else {
    subObjects[objIdx]->getColor(ray, scattered);
  }
  return false;
}
