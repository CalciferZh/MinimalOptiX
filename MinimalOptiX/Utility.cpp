#include "Utility.h"


namespace Utility {
  float randReal() {
    static auto randSeed = std::minstd_rand(std::random_device{}());
    static auto randGen = std::uniform_real_distribution<float>(0, 1);
    return randGen(randSeed);
  }

  float schlick(float cosine, float refIdx) {
    float r = (1 - refIdx) / (1 + refIdx);
    r *= r;
    return r + (1 - r) * pow((1 - cosine), 5);
  }

  optix::float3 randInUnitDisk() {
    static optix::float3 ones = { 1.f, 1.f, 0.f };
    optix::float3 rand;
    do {
      rand = { randReal(), randReal(), 0.f };
      rand *= 2;
      rand -= ones;
    } while (optix::dot(rand, rand) >= 1.0);
    return rand;
  }

  void randInUnitSphere(optix::float3& rand) {
    static optix::float3 ones = { 1.f, 1.f, 1.f };
    do {
      rand = { randReal(), randReal(), randReal() };
      rand *= 2;
      rand -= ones;
    } while (optix::length(rand) >= 1.0);
  }

  optix::float3 reflect(optix::float3& v, optix::float3& n) {
    return (v - 2 * optix::dot(v, n) * n);
  }

  optix::float3 randInUnitSphere() {
    optix::float3 rand;
    static optix::float3 ones = { 1.f, 1.f, 1.f };
    do {
      rand = { randReal(), randReal(), randReal() };
      rand *= 2;
      rand -= ones;
    } while (optix::length(rand) >= 1.0);
    return rand;
  }


  bool refract(optix::float3& v, optix::float3& n, float refRatio, optix::float3& refracted) {
    optix::float3 vUnit = optix::normalize(v);
    float dt = optix::dot(vUnit, n);
    float discriminant = 1.0 - refRatio * refRatio * (1 - dt * dt);
    if (discriminant > 0) {
      refracted = refRatio * (vUnit - n * dt) - n * sqrt(discriminant);
      return true;
    }
    return false;
  }
}
