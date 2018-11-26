#pragma once

#include <QtWidgets/QMainWindow>
#include <QKeyEvent>
#include <QDateTime>
#include <QString>
#include <QGraphicsScene>
#include <QMessageBox>
#include "ui_MinimalOptiX.h"
#include "Context.h"


class MinimalOptiX : public QMainWindow {
	Q_OBJECT

public:
  enum SceneNum { SCENE_0 };

	//ruction
	MinimalOptiX(QWidget *parent = Q_NULLPTR);

	// Utilities
	void updateScene();
  void drawSomething();
  void setupContext();
  void setupScene(SceneNum num);
  void float3toQColor(optix::float3& f, QColor& color);

	// GUI Components
	QGraphicsScene scene;
	QImage canvas;
  Context context;

  // Attributes
  int fixedWidth = 1024;
  int fixedHeight = 512;

  // User Interface
  void keyPressEvent(QKeyEvent* e);

private:
	Ui::MinimalOptiXClass ui;
};
