#include <optix_world.h>

using namespace optix;

__device__ __inline__ uchar4 make_color(const float3& c) {
  return make_uchar4(
    static_cast<unsigned char>(__saturatef(c.x)*255.99f),
    static_cast<unsigned char>(__saturatef(c.y)*255.99f),
    static_cast<unsigned char>(__saturatef(c.z)*255.99f),
    255u
  );
}

// Generate random unsigned int in [0, 2^24)
__device__ __inline__ unsigned int lcg(int& seed) {
  const unsigned int LCG_A = 1664525u;
  const unsigned int LCG_C = 1013904223u;
  seed = (LCG_A * seed + LCG_C);
  return seed & 0x00FFFFFF;
}

// Generate random float in [0, 1)
__device__ __inline__ float rand(int& seed) {
  return ((float)lcg(seed) / (float)0x01000000);
}

__device__ __inline__ float3 randInUnitSphere(int& seed) {
  static float3 ones = { 1.f, 1.f, 1.f };
  float3 res;
  do {
    res = make_float3(rand(seed), rand(seed), rand(seed)) * 2 - ones;
  } while (length(res) >= 1.0);
  return res;
}

__device__ __inline__ float3 reflect(float3& v, float3& n) {
  return (v - 2 * dot(v, n) * n);
}

__device__ __inline__ bool refract(float3& v, float3& n, float refRatio, float3& refracted) {
  float dt = dot(v, n);
  float discriminant = 1.0 - refRatio * refRatio * (1 - dt * dt);
  if (discriminant > 0) {
    refracted = refRatio * (v - n * dt) - n * sqrt(discriminant);
    return true;
  }
  return false;
}

__device__ __inline__ float schlick(float cosine, float refIdx) {
  float r = (1 - refIdx) / (1 + refIdx);
  r *= r;
  return r + (1 - r) * powf((1 - cosine), 5.f);
}
