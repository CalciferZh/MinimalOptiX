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
#include "ui_MinimalOptiX.h"
#include "utils_host.h"
#include "structures.h"
#include "scene.h"


class MinimalOptiX : public QMainWindow {
	Q_OBJECT

public:
  enum SceneId { SCENE_BASIC_TEST, SCENE_MESH_TEST, SCENE_COFFEE };
  enum UpdateSource { OUTPUT_BUFFER, ACCU_BUFFER };

	//ruction
	MinimalOptiX(QWidget *parent = Q_NULLPTR);

	// Utilities
  void compilePtx();
  void setupScene(SceneId sceneId);
  void setupContext();
	void update(UpdateSource source);

	// Components
	QGraphicsScene qgscene;
	QImage canvas;
  optix::Context context;

  uint fixedWidths[3] = { 1024u, 800u, 800u };
  uint fixedHeights[3] = { 512u, 1000u, 1000u };
  // Attributes

  uint fixedWidth;
  uint fixedHeight;
  uint nSuperSampling = 1u;
  uint rayMaxDepth = 64;
  uint defaultNScatter = 32;
  float rayMinIntensity = 0.01f;
  float rayEpsilonT = 0.01f;
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

  // scene
  SceneId scendId = SCENE_COFFEE;

  // for animation
  const float gravity = 9.8f;
  const float attenuationCoef = 0.95f;
  SphereParams sphereParams[3] = { { 0.5f, { 0.f, 0.5f, -1.f }, { 0.f, 0.f, 0.f } } ,
                                   { 0.4f, { 1.f, 0.5f, -1.f }, { 0.f, 0.5f, 0.f } } ,
                                   { 0.3f, { -1.f, 0.5f, -1.f }, { 0.f, -1.5f, 0.f } } };

  // User Interface
  void keyPressEvent(QKeyEvent* e);

  void record(int frames, const char* filename);

private:
	Ui::MinimalOptiXClass ui;

  // 1s = 1000ticks
  void animate(int ticks);
  void move(SphereParams& param, float time);
  void render(SceneId sceneId);
};
