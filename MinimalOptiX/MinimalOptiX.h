#pragma once

#include <QtWidgets/QMainWindow>
#include <QKeyEvent>
#include <QDateTime>
#include <QString>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QDebug>
#include <optix_world.h>
#include <unordered_map>
#include <exception>
#include <map>
#include "ui_MinimalOptiX.h"
#include "utils_host.h"
#include "structures.h"
#include "scene.h"


class MinimalOptiX : public QMainWindow {
	Q_OBJECT

public:
  enum SceneId { SCENE_BASIC_TEST, SCENE_MESH_TEST, SCENE_COFFEE, SCENE_BEDROOM };
  enum UpdateSource { OUTPUT_BUFFER, ACCU_BUFFER };
  enum RayType { RAY_TYPE_RADIANCE, RAY_TYPE_SHADOW };

	// construction
	MinimalOptiX(QWidget *parent = Q_NULLPTR);

	// Utilities
  void compilePtx();
  void setupContext();
  void setupScene(SceneId sceneId);
  void setupScene(const char* sceneName);
	void updateContent(UpdateSource source, float nAccumulation, bool clearBuffer);
  void saveCurrentFrame(bool popUpDialog);

	// components
	QGraphicsScene qgscene;
	QImage canvas;
  optix::Context context;
  
  // attributes
  SceneId scendId = SCENE_BEDROOM;
  uint fixedWidths[4] = { 1024u, 800u, 800u, 960u };
  uint fixedHeights[4] = { 512u, 1000u, 1000u, 540u };
  uint fixedWidth;
  uint fixedHeight;
  uint nSuperSampling = 512u;
  uint rayMaxDepth = 256;
  uint defaultNScatter = 32;
  float rayMinIntensity = 0.001f;
  float rayEpsilonT = 0.001f;
  optix::Aabb aabb;
  std::map<std::string, std::string> ptxStrs;
  std::string baseSceneFolder = "scenes/";

  std::string camCuFileName = "camera.cu";
  std::string exCuFileName = "exception.cu";
  std::string mtlCuFileName = "material.cu";
  std::string msCuFileName = "miss.cu";
  std::string geoCuFileName = "geometry.cu";
  std::vector<std::string> cuFiles = { camCuFileName, exCuFileName, mtlCuFileName, msCuFileName, geoCuFileName };

  // camera
  CamParams camParams;
  optix::float3 lookFrom = { 0.f, 1.f, 2.f };
  optix::float3 lookAt = { 0.f, 0.f, -1.f };
  optix::float3 up = { 0.f, 1.f, 0.f };

  // animation
  const float gravity = 9.8f;
  const float attenuationCoef = 0.95f;
  SphereParams sphereParams[3] = { { 0.5f, { 0.f, 0.5f, -1.f }, { 0.f, 0.f, 0.f } } ,
                                   { 0.4f, { 1.f, 0.5f, -1.f }, { 0.f, 0.5f, 0.f } } ,
                                   { 0.3f, { -1.f, 0.5f, -1.f }, { 0.f, -1.5f, 0.f } } };

  // user interface
  void keyPressEvent(QKeyEvent* e);
  void record(int frames, const char* filename);

private:
	Ui::MinimalOptiXClass ui;

  // 1s = 1000ticks
  void animate(int ticks);
  void move(SphereParams& param, float time);
  void render(SceneId sceneId);
};
