#include <optix_world.h>
#include "Structures.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtDeclareVariable(float, rayEpsilonT, , );
rtDeclareVariable(float3, origin, , );
rtDeclareVariable(float3, u, , );
rtDeclareVariable(float3, v, , );
rtDeclareVariable(float3, w, , );
rtDeclareVariable(float3, scrLowerLeftCorner, , );
rtDeclareVariable(uint, rayTypeRadience, , );
rtDeclareVariable(uint, rayTypeShadow, , );
rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );
rtDeclareVariable(rtObject, topObject, , );
rtBuffer<uchar, 2> outputBuffer;

RT_PROGRAM void pinholeCamera() {
  size_t2 screen = outputBuffer.size();
  float2 xy = make_float2(launchIdx) / make_float2(screen) * 2.f - 1.f;
  float3 rayOri = origin;
  float3 rayDir = normalize(xy.x * u + xy.y * v + w);
  Ray ray(rayOri, rayDir, rayTypeRadience, rayEpsilonT);
  PayloadRadiance pldR;
  pldR.color = make_float3(1.f);
  pldR.intensity = 1.f;
  pldR.depth = 0;
  rtTrace(topObject, ray, pldR);
  outputBuffer[launchIdx] = make_uchar4(
    static_cast<unsigned char>(__saturatef(pldR.color.z)*255.99f),
    static_cast<unsigned char>(__saturatef(pldR.color.y)*255.99f),
    static_cast<unsigned char>(__saturatef(pldR.color.x)*255.99f),
    255u
  );
}
