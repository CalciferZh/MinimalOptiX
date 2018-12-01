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
#include "Camera.h"
#include "Utils.h"


class MinimalOptiX : public QMainWindow {
	Q_OBJECT

public:
  enum SceneNum { SCENE_0 };

	//ruction
	MinimalOptiX(QWidget *parent = Q_NULLPTR);

	// Utilities
  void compilePtx();
	void updateScene();
  void setupContext();
  void setupScene(SceneNum num);

	// Components
	QGraphicsScene scene;
	QImage canvas;
  optix::Context context;
  std::map<std::string, std::string> ptxStrs;
  std::vector<std::string> cuFiles = { "Camera.cu", "Exception.cu", "Material.cu", "MissProgram.cu", "Geometry.cu" };

  // Attributes
  uint fixedWidth = 1024u;
  uint fixedHeight = 512u;

  // User Interface
  void keyPressEvent(QKeyEvent* e);

private:
	Ui::MinimalOptiXClass ui;
};
