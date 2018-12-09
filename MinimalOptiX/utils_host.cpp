#pragma once

#include "utils_host.h"

void getStrFromFile(std::string& content, std::string& fileName) {
  std::ifstream file(fileName);
  std::stringstream buffer;
  buffer << file.rdbuf();
  content = buffer.str();
}

void getPtxStrFromCuStr(std::string& cuStr, std::string& ptxStr, std::string& fileName) {
  nvrtcProgram prog = 0;
  nvrtcCreateProgram(&prog, cuStr.c_str(), fileName.c_str(), 0, NULL, NULL);
  std::vector<const char*> options;
  options.push_back("-IC:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v9.1/include");
  options.push_back("-IC:/ProgramData/NVIDIA Corporation/OptiX SDK 5.1.1/include/optixu");
  options.push_back("-IC:/ProgramData/NVIDIA Corporation/OptiX SDK 5.1.1/include");
  options.push_back("-arch");
  options.push_back("compute_30");
  options.push_back("-use_fast_math");
  options.push_back("-default-device");
  options.push_back("-rdc");
  options.push_back("true");
  options.push_back("-D__x86_64");

  const nvrtcResult compileRes = nvrtcCompileProgram(prog, (int)options.size(), options.data());

  size_t logSize = 0;
  nvrtcGetProgramLogSize(prog, &logSize);
  std::string nvrtcLog;
  if (logSize > 1) {
    nvrtcLog.resize(logSize);
    nvrtcGetProgramLog(prog, &nvrtcLog[0]);
  }
  if (compileRes != NVRTC_SUCCESS) {
    throw std::runtime_error("NVRTC Compilation failed.\n" + nvrtcLog);
  }

  // Retrieve PTX code
  size_t ptx_size = 0;
  nvrtcGetPTXSize(prog, &ptx_size);
  ptxStr.resize(ptx_size);
  nvrtcGetPTX(prog, &ptxStr[0]);

  // Cleanup
  nvrtcDestroyProgram(&prog);
}

void cuFileToPtxStr(std::string& fileName, std::string& ptxStr) {
  std::string cuStr;
  getStrFromFile(cuStr, fileName);
  getPtxStrFromCuStr(cuStr, ptxStr, fileName);
}

void setQuadParams(optix::float3& anchor, optix::float3& v1, optix::float3& v2, QuadParams& quadParams) {
  optix::float3 normal = normalize(optix::cross(v2, v1));
  float d = dot(normal, anchor);
  v1 /= dot(v1, v1);
  v2 /= dot(v2, v2);
  optix::float4 plane = make_float4(normal, d);
  quadParams.v1 = v1;
  quadParams.v2 = v2;
  quadParams.plane = plane;
  quadParams.anchor = anchor;
}

void setCamParams(optix::float3& lookFrom, optix::float3& lookAt, optix::float3& up, float vFoV, float aspect, CamParams& camParams) {
  static const float pi = 3.141592653589793238462643383279502884;
  float theta = vFoV * pi / 180;
  float halfHeight = tan(theta / 2);
  float halfWidth = aspect * halfHeight;
  camParams.origin = lookFrom;
  optix::float3 w = optix::normalize(lookFrom - lookAt);
  optix::float3 u = optix::normalize(optix::cross(up, w));
  optix::float3 v = optix::cross(w, u);
  camParams.srcLowerLeftCorner = camParams.origin - halfWidth * u - halfHeight * v - w;
  camParams.horizontal = 2 * halfWidth * u;
  camParams.vertical = 2 * halfHeight * v;
}

void initDisneyParams(DisneyParams& disneyParams) {
  disneyParams.color = optix::make_float3(1.0f, 1.0f, 1.0f);
  disneyParams.emission = optix::make_float3(0.0f);
  disneyParams.metallic = 0.0;
  disneyParams.subsurface = 0.0f;
  disneyParams.specular = 0.5f;
  disneyParams.roughness = 0.5f;
  disneyParams.specularTint = 0.0f;
  disneyParams.anisotropic = 0.0f;
  disneyParams.sheen = 0.0f;
  disneyParams.sheenTint = 0.5f;
  disneyParams.clearcoat = 0.0f;
  disneyParams.clearcoatGloss = 1.0f;
  disneyParams.brdf = NORMAL;
  disneyParams.albedoID = RT_TEXTURE_ID_NULL;
}

int randSeed() {
  static auto randSeed = std::minstd_rand(std::random_device{}());
  static auto randGen = std::uniform_real_distribution<float>(-1.f, 1.f);
  return int(randGen(randSeed) * (float)std::numeric_limits<int>::max());
}
