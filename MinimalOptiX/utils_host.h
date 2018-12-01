#pragma once

#include <optix_world.h>
#include <nvrtc.h>
#include <string>
#include <fstream>
#include <sstream>
#include <exception>
#include <vector>

void getStrFromFile(std::string& content, std::string& fileName);

void getPtxStrFromCuStr(std::string& cuStr, std::string& ptxStr, std::string& fileName);

void cuFileToPtxStr(std::string& fileName, std::string& ptxStr);
