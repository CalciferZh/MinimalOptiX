#include <optix_world.h>

using namespace optix;

rtDeclareVariable(float3, badColor, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void exception() {
  outputBuffer[launchIdx] = make_uchar4(
    static_cast<unsigned char>(__saturatef(badColor.z)*255.99f),
    static_cast<unsigned char>(__saturatef(badColor.y)*255.99f),
    static_cast<unsigned char>(__saturatef(badColor.x)*255.99f),
    255u
  );
}
