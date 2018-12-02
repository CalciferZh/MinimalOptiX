#include <optix_world.h>
#include "Structures.h"

using namespace optix;

rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );
rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(float, t, rtIntersectionDistance, );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
rtDeclareVariable(float3, mtlColor, , );
rtBuffer<Light> lights;

RT_PROGRAM void phong() {
  float3 tmpColor = { 0.f, 0.f, 0.f };
  for (int i = 0; i < lights.size(); ++i) {
    float3 P = ray.origin + t * ray.direction;
    float3 PtoL = normalize(lights[i].position - P);
    float brightness = dot(geoNormal, PtoL);
    if (brightness > 0) {
      tmpColor += brightness * lights[i].color;
    }
  }
  pldR.color *= (tmpColor / lights.size());
  pldR.color *= mtlColor;
}
