#pragma once

#include "utils_host.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
}

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

int randSeed() {
  static auto randSeed = std::minstd_rand(std::random_device{}());
  static auto randGen = std::uniform_real_distribution<float>(-1.f, 1.f);
  return int(randGen(randSeed) * (float)std::numeric_limits<int>::max());
}

void generateVideo(std::vector<QImage>& images, const char* output_path) {
  int ret;
  const int width = 1024;
  const int height = 512;
  const int in_linesize[1] = { 3 * width };
  AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
  if (!codec) {
    throw std::runtime_error("Codec init failed.");
  }

  SwsContext* sws_context = NULL;
  sws_context = sws_getCachedContext(sws_context,
    width, height, AV_PIX_FMT_RGB24,
    width, height, AV_PIX_FMT_YUV420P,
    0, 0, 0, 0);
  if (!sws_context) {
    throw std::runtime_error("Create sws context failed.");
  }

  AVCodecContext* c = avcodec_alloc_context3(codec);
  if (!c) {
    throw std::runtime_error("Allocate video codec context failed.");
  }

  c->bit_rate = 400000;
  c->width = width;
  c->height = height;
  c->time_base.num = 1;
  //c->time_base.den = 1;
  c->time_base.den = 25;
  c->gop_size = 10;
  c->max_b_frames = 1;
  c->pix_fmt = AV_PIX_FMT_YUV420P;
  if (avcodec_open2(c, codec, NULL) < 0) {
    throw std::runtime_error("Open codec failed.");
  }

  FILE* file = fopen(output_path, "wb");
  if (!file) {
    throw std::runtime_error("Open output file failed.");
  }

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    throw std::runtime_error("Allocate video frame failed.");
  }
  frame->format = c->pix_fmt;
  frame->width = c->width;
  frame->height = c->height;

  ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 32);
  if (ret < 0) {
    throw std::runtime_error("Allocate raw picture buffer failed.");
  }

  AVPacket pkt;

  uint8_t* rgb = NULL;

  for (int pts = 0; pts < images.size(); pts++) {
    frame->pts = pts;
    rgb = images[pts].bits();
    int got_output;
    sws_scale(sws_context, (const uint8_t * const *)&rgb, in_linesize, 0,
      c->height, frame->data, frame->linesize);
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
    if (ret < 0) {
      throw std::runtime_error("Encoding frame failed.");
    }
    if (got_output) {
      fwrite(pkt.data, 1, pkt.size, file);
      av_packet_unref(&pkt);
    }
  }
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };
  int got_output;
  do {
    fflush(stdout);
    ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
    if (ret < 0) {
      fprintf(stderr, "Error encoding frame\n");
      exit(1);
    }
    if (got_output) {
      fwrite(pkt.data, 1, pkt.size, file);
      av_packet_unref(&pkt);
    }
  } while (got_output);
  fwrite(endcode, 1, sizeof(endcode), file);
  fclose(file);
  avcodec_close(c);
  av_free(c);
  av_freep(&frame->data[0]);
  av_frame_free(&frame);
}