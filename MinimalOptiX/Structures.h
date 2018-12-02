#include <optix_world.h>

struct PayloadRadiance {
  optix::float3 color;
  float depth;
  int randSeed;
};

struct Light {
  optix::float3 color;
};
