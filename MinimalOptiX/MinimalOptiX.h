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


class MinimalOptiX : public QMainWindow {
	Q_OBJECT

public:
  enum SceneNum { SCENE_0 };
  enum UpdateSource { OUTPUT_BUFFER, ACCU_BUFFER };

	//ruction
	MinimalOptiX(QWidget *parent = Q_NULLPTR);

	// Utilities
  void compilePtx();
  void setupScene(SceneNum num);
  void setupContext();
	void update(UpdateSource source);

	// Components
	QGraphicsScene scene;
	QImage canvas;
  optix::Context context;

  // Attributes
  uint fixedWidth = 1024u;
  uint fixedHeight = 512u;
  uint nSuperSampling = 32u;
  uint rayMaxDepth = 64;
  uint defaultNScatter = 32;
  float rayMinIntensity = 0.01f;
  float rayEpsilonT = 0.01f;
  std::map<std::string, std::string> ptxStrs;

  std::string camCuFileName = "camera.cu";
  std::string exCuFileName = "exception.cu";
  std::string mtlCuFileName = "material.cu";
  std::string msCuFileName = "miss.cu";
  std::string geoCuFileName = "geometry.cu";
  std::vector<std::string> cuFiles = { camCuFileName, exCuFileName, mtlCuFileName, msCuFileName, geoCuFileName };

  // User Interface
  void keyPressEvent(QKeyEvent* e);

private:
	Ui::MinimalOptiXClass ui;
};
