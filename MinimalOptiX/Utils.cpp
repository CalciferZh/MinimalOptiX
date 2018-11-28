#pragma once

#include "Utils.h"

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
