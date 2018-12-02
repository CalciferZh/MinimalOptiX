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
rtDeclareVariable(float3, envLightColor, , );
rtBuffer<Light> lights;

RT_PROGRAM void phong() {
  float3 tmpColor = Ka * envLightColor;
  for (int i = 0; i < lights.size(); ++i) {
    float3 P = ray.origin + t * ray.direction;
    float3 PtoL = normalize(lights[i].position - P);
    float brightness = dot(geoNormal, PtoL);
    if (brightness > 0) {
      tmpColor += brightness * lights[i].color;
    }
  }
  pldR.color *= tmpColor;
  pldR.color *= mtlColor;
}
