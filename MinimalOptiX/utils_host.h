#pragma once

#include <optix_world.h>
#include <nvrtc.h>
#include <string>
#include <fstream>
#include <sstream>
#include <exception>
#include <vector>
#include <limits>
#include <random>
#include <QImage>
#include "structures.h"

void getStrFromFile(std::string& content, std::string& fileName);

void getPtxStrFromCuStr(std::string& cuStr, std::string& ptxStr, std::string& fileName);

void cuFileToPtxStr(std::string& fileName, std::string& ptxStr);

void setQuadParams(optix::float3& anchor, optix::float3& v1, optix::float3& v2, QuadParams& quadParams);

void setCamParams(
  optix::float3& lookFrom, optix::float3& lookAt, optix::float3& up,
  float vFoV, float aspect, float aperture, float focus, CamParams& camParams
);

void initDisneyParams(DisneyParams& disneyParams);

void generateVideo(std::vector<QImage>& images, const char*);

int randSeed();
