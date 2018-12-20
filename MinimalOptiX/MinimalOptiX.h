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
  enum SceneId {
    SCENE_SPHERES,
    SCENE_COFFEE,
    SCENE_BEDROOM,
    SCENE_DININGROOM,
    SCENE_STORMTROOPER,
    SCENE_SPACESHIP,
    SCENE_CORNELL, 
    SCENE_HYPERION, 
    SCENE_DRAGON
  };
  enum RayType { RAY_TYPE_RADIANCE, RAY_TYPE_SHADOW };

	// construction
	MinimalOptiX(QWidget *parent = Q_NULLPTR);

	// utilities
  void compilePtx();
  void setupContext();
  void setupScene();
  void setupScene(const char* sceneName);
  void renderScene(bool autoSave = false, std::string fileNamePrefix = "");
	void updateContent(float nAccumulation, bool clearBuffer);
  void saveCurrentFrame(bool popUpDialog, std::string fileNamePrefix = "");
  void imageDemo();
  void videoDemo();

	// components
	QGraphicsScene qgscene;
	QImage canvas;
  optix::Context context;
  optix::Aabb aabb;
  std::map<std::string, std::string> ptxStrs;
  std::string baseSceneFolder = "scenes/";
  std::string camCuFileName = "camera.cu";
  std::string exCuFileName = "exception.cu";
  std::string mtlCuFileName = "material.cu";
  std::string msCuFileName = "miss.cu";
  std::string geoCuFileName = "geometry.cu";
  std::vector<std::string> cuFiles = {
    camCuFileName, exCuFileName, mtlCuFileName, msCuFileName, geoCuFileName
  };
  
  // attributes
  SceneId sceneId;
  uint fixedWidth = 1920u;
  uint fixedHeight = 1080u;
  uint nSuperSampling = 32u;
  uint rayMaxDepth = 256u;
  float rayMinIntensity = 0.001f;
  float rayEpsilonT = 0.001f;

  // camera
  CamParams camParams;
  optix::float3 lookFrom;
  optix::float3 lookAt;
  optix::float3 up;

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
};
