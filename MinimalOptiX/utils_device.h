#include <optix_world.h>

using namespace optix;

template<unsigned int N>
__device__ __inline__ unsigned int tea(unsigned int val0, unsigned int val1) {
  unsigned int v0 = val0;
  unsigned int v1 = val1;
  unsigned int s0 = 0;

  for (unsigned int n = 0; n < N; n++) {
    s0 += 0x9e3779b9;
    v0 += ((v1<<4)+0xa341316c)^(v1+s0)^((v1>>5)+0xc8013ea4);
    v1 += ((v0<<4)+0xad90777d)^(v0+s0)^((v0>>5)+0x7e95761e);
  }

  return v0;
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

__device__ __inline__ uchar4 make_color(const float3& c) {
  return make_uchar4(
    static_cast<unsigned char>(__saturatef(c.x)*255.99f),
    static_cast<unsigned char>(__saturatef(c.y)*255.99f),
    static_cast<unsigned char>(__saturatef(c.z)*255.99f),
    255u
  );
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


// Plane intersection -- used for refining triangle hit points.  Note
// that this skips zero denom check (for rays perpindicular to plane normal)
// since we know that the ray intersects the plane.
__device__ __inline__ float intersectPlane(
  const float3& origin,
  const float3& direction,
  const float3& normal,
  const float3& point) {
  // Skipping checks for non-zero denominator since we know that ray intersects this plane
  return -(dot(normal, origin - point)) / dot(normal, direction);
}

// Offset the hit point using integer arithmetic
__device__ __inline__ float3 offset(const float3& hit_point, const float3& normal) {
  const float epsilon = 1.0e-4f;
  const float offset  = 4096.0f * 2.0f;
  float3 offset_point = hit_point;
  if ((__float_as_int(hit_point.x) & 0x7fffffff) < __float_as_int(epsilon)) {
    offset_point.x += epsilon * normal.x;
  } else {
    offset_point.x = __int_as_float(__float_as_int(offset_point.x) + int(copysign(offset, hit_point.x) * normal.x));
  }

  if((__float_as_int(hit_point.y ) & 0x7fffffff) < __float_as_int(epsilon)) {
    offset_point.y += epsilon * normal.y;
  } else {
    offset_point.y = __int_as_float(__float_as_int(offset_point.y) + int(copysign(offset, hit_point.y) * normal.y) );
  }

  if((__float_as_int(hit_point.z) & 0x7fffffff) < __float_as_int(epsilon)) {
    offset_point.z += epsilon * normal.z;
  } else {
    offset_point.z = __int_as_float(__float_as_int(offset_point.z) + int(copysign(offset, hit_point.z) * normal.z));
  }
  return offset_point;
}

// Refine the hit point to be more accurate and offset it for reflection and
// refraction ray starting points.
__device__ __inline__ void refineHitpoint(
  const float3& original_hit_point,
  const float3& direction,
  const float3& normal,
  const float3& p,
  float3& back_hit_point,
  float3& front_hit_point)
{
  // Refine hit point
  float  refined_t = intersectPlane(original_hit_point, direction, normal, p);
  float3 refined_hit_point = original_hit_point + refined_t * direction;

  // Offset hit point
  if(dot(direction, normal) > 0.0f) {
    back_hit_point = offset(refined_hit_point, normal);
    front_hit_point = offset(refined_hit_point, -normal);
  } else {
    back_hit_point = offset(refined_hit_point, -normal);
    front_hit_point = offset(refined_hit_point,  normal);
  }
}

__device__ __inline__ float GTR1(float NDotH, float a) {
  if (a >= 1.f) {
    return (1.f / M_PIf);
  }
  float a2 = a * a;
  float t = 1.f + (a2 - 1.f) * NDotH * NDotH;
  return (a2 - 1.0f) / (M_PIf * logf(a2) * t);
}

__device__ __inline__ float GTR2(float NDotH, float a) {
  float a2 = a * a;
  float t = 1.f + (a2 - 1.f) * NDotH * NDotH;
  return a2 / (M_PIf * t*t);
}

__device__ __inline__ float GTR2_Aniso(float ndoth, float hdotx, float hdoty, float ax, float ay) {
	float hdotxa2 = (hdotx / ax);
	hdotxa2 *= hdotxa2;
	float hdotya2 = (hdoty / ay);
	hdotya2 *= hdotya2;
	float denom = hdotxa2 + hdotya2 + ndoth * ndoth;
	return denom > 1e-5 ? (1.f / (M_PIf * ax * ay * denom * denom)) : 0.f;
}

__device__ __inline__ float SchlickFresnel(float u) {
  float m = clamp(1.f-u, 0.f, 1.f);
  float m2 = m * m;
  return m2 * m2 * m;
}

__device__ __inline__ float smithG_GGX(float NDotv, float alphaG) {
  float a = alphaG * alphaG;
  float b = NDotv * NDotv;
  return 1.f / (NDotv + sqrtf(a + b - a * b));
}
