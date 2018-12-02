#include <optix_world.h>
#include "Structures.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );
rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );

rtDeclareVariable(uint, rayTypeRadience, , );
rtDeclareVariable(rtObject, topObject, , );
rtDeclareVariable(float, rayEpsilonT, , );
rtDeclareVariable(float3, origin, , );
rtDeclareVariable(float3, horizontal, , );
rtDeclareVariable(float3, vertical, , );
rtDeclareVariable(float3, scrLowerLeftCorner, , );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void pinholeCamera() {
  float2 xy = make_float2(launchIdx) / make_float2(launchDim);
  float3 rayOri = origin;
  float3 rayDir = normalize(scrLowerLeftCorner + xy.x * horizontal + xy.y * vertical - origin);
  Ray ray(rayOri, rayDir, rayTypeRadience, rayEpsilonT);
  PayloadRadiance pldR;
  pldR.color = make_float3(1.f);
  pldR.intensity = 1.f;
  rtTrace(topObject, ray, pldR);
  outputBuffer[launchIdx] = make_uchar4(
    static_cast<unsigned char>(__saturatef(pldR.color.z)*255.99f),
    static_cast<unsigned char>(__saturatef(pldR.color.y)*255.99f),
    static_cast<unsigned char>(__saturatef(pldR.color.x)*255.99f),
    255u
  );
}
