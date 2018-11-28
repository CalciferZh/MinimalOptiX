#include <optix_world.h>

struct PayloadRadiance {
  optix::float3 color;
  float intensity;
  int depth;
};
