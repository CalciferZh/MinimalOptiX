#pragma once

#include "Ray.h"


class MissProgram {
public:
  MissProgram() = default;
  virtual void operator()(Ray& ray);
  optix::float3 color = { 1.0f, 1.0f, 1.0f };
};


class VerticalGradienMissProgram : public MissProgram {
public:
  VerticalGradienMissProgram() = default;
  void operator()(Ray& ray) override;

  optix::float3 color1 = { 0.5f, 0.7f, 1.0f };
  optix::float3 color2 = { 1.0f, 1.0f, 1.0f };
};

