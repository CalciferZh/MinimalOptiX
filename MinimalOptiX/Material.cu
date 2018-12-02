#include <optix_world.h>
#include "Structures.h"

using namespace optix;

rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );
rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(float, t, rtIntersectionDistance, );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
rtDeclareVariable(float3, mtlColor, , );
rtDeclareVariable(float3, Ka, , );
rtDeclareVariable(float3, Kd, , );
rtDeclareVariable(float3, Ks, , );
rtDeclareVariable(float, phongExp, , );
rtDeclareVariable(float3, envLightColor, , );
rtBuffer<Light> lights;

RT_PROGRAM void phong() {
  float3 tmpColor = Ka * envLightColor;
  for (int i = 0; i < lights.size(); ++i) {
    float3 P = ray.origin + t * ray.direction;
    float3 L = normalize(lights[i].position - P);
    float nDl = dot(geoNormal, L);
    if (nDl > 0) {
      tmpColor += Kd * nDl * lights[i].color;
      float3 H = normalize(L - ray.direction);
      float nDh = dot(geoNormal, H);
      if (nDh > 0) {
        tmpColor += Ks * lights[i].color * pow(nDh, phongExp);
      }
    }
  }
  pldR.color *= tmpColor;
  pldR.color *= mtlColor;
}
