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

struct VideoParams {
  // static
  std::vector<optix::GeometryInstance> spheres;
  // animation
  const float gravity = 49.f;
  const float attenuationCoef = 0.95f;
  // dynamic
  float angle{ 0.0 };
  optix::float3 lookAt { 0.f, 0.f, 0.f };
  optix::float3 up { 0.f, 1.f, 0.f };
  std::vector<SphereParams> spheresParams;
};

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
    SCENE_DRAGON,
    SCENE_SPHERES_VIDEO
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


  VideoParams videoParams;
  // user interface
  void keyPressEvent(QKeyEvent* e);
  void record(int frames, const char* filename);

private:
	Ui::MinimalOptiXClass ui;

  void animate(float time);
  void move(SphereParams& param, float time);
  void setUpVideo(int nSpheres);
  void updateVideo();

  optix::GeometryInstance buildLight(optix::float3 anchor, optix::float3 v1, optix::float3 v2, optix::Program& quadIntersect, optix::Program& quadBBox, optix::Program& lightMtl);
};
