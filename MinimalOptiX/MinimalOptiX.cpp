#include "MinimalOptiX.h"

MinimalOptiX::MinimalOptiX(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
  setFixedSize(fixedWidth, fixedHeight);
  setWindowFlags(Qt::MSWindowsFixedSizeDialogHint);

  ui.view->setFrameStyle(0);
  ui.view->setFixedSize(fixedWidth, fixedHeight);
  ui.view->setSceneRect(0, 0, fixedWidth, fixedHeight);
  ui.view->fitInView(0, 0, fixedWidth, fixedHeight, Qt::KeepAspectRatio);
  ui.view->setScene(&scene);

  canvas = QImage(ui.view->size(), QImage::Format_RGB888);

  setupContext();
  context.launch();
  updateScene();
}

void MinimalOptiX::updateScene() {
  QColor color;
  for (int i = 0; i < fixedWidth; ++i) {
    for (int j = 0; j < fixedHeight; ++j) {
      float3toQColor(context.displayBuffer[i][j], color);
      canvas.setPixelColor(i, fixedHeight - j - 1, color);
    }
  }
  QPixmap tmpPixmap = QPixmap::fromImage(canvas);
  scene.clear();
  scene.addPixmap(tmpPixmap);
  ui.view->update();
}

void MinimalOptiX::float3toQColor(optix::float3& f, QColor& color) {
  color.setRgb((int)(sqrt(f.x) * 255), (int)(sqrt(f.y) * 255), (int)(sqrt(f.z) * 255));
}

void MinimalOptiX::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Space) {
    canvas.save(QString::number(QDateTime::currentMSecsSinceEpoch()) + QString(".png"));
    QMessageBox::information(
      this,
      "Save",
      "Image saved!",
      QMessageBox::Ok
    );
  }
}

void MinimalOptiX::setupContext() {
  // Camera
  optix::float3 lookFrom = { 0.f, 1.5f, 1.5f };
  optix::float3 lookAt = { 0.f, 0.f, -1.f };
  optix::float3 up = { 0.f, 1.f, 0.f };
  context.camera.set(
    lookFrom, lookAt, up, 90, (float)fixedWidth / (float)fixedHeight
  );

  // Miss Program
  context.mp = std::make_shared<VerticalGradienMissProgram>();

  // Screen
  context.setSize(fixedWidth, fixedHeight);

  // Scene
  setupScene(SCENE_0);
}

void MinimalOptiX::setupScene(SceneNum num) {
  if (num == SCENE_0) {
    auto rto = std::make_shared<RtObject>();
    rto->geometry = std::make_shared<Sphere>(optix::float3{ 0.f, 0.f, -1.f }, 0.5f);
    rto->material = std::make_shared<LambertianMaterial>(optix::float3{ 0.1f, 0.2f, 0.5f });
    context.world.emplace_back(rto);

    rto = std::make_shared<RtObject>();
    rto->geometry = std::make_shared<Triangle>(
      optix::float3{ 2.f, -0.5f, -2.f },
      optix::float3{ -2.f, -0.5f, -2.f },
      optix::float3{ 0.f, -0.5f, 1.f }
    );
    rto->material = std::make_shared<LambertianMaterial>(optix::float3{ 0.8f, 0.8f, 0.f });
    context.world.emplace_back(rto);

    rto = std::make_shared<RtObject>();
    rto->geometry = std::make_shared<Sphere>(optix::float3{ 1.f, 0.f, -1.f }, 0.5f);
    rto->material = std::make_shared<MetalMaterial>(optix::float3{ 0.8f, 0.6f, 0.2f });
    context.world.emplace_back(rto);

    rto = std::make_shared<RtObject>();
    rto->geometry = std::make_shared<Sphere>(optix::float3{ -1.f, 0.f, -1.f }, 0.5f);
    rto->material = std::make_shared<DielectricMaterial>(1.5f);
    context.world.emplace_back(rto);

    rto = std::make_shared<RtObject>();
    rto->geometry = std::make_shared<Sphere>(optix::float3{ -1.f, 0.f, -1.f }, -0.45);
    rto->material = std::make_shared<DielectricMaterial>(1.5);
    context.world.emplace_back(rto);
  }
}
