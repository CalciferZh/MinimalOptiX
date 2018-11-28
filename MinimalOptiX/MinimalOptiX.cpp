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
  setupScene(SCENE_0);
  context->validate();
  context->launch(0, fixedWidth, fixedHeight);
  updateScene();
}

void MinimalOptiX::updateScene() {
  uchar* bufferData = (uchar*)context["outputBuffer"]->getBuffer()->map();

  QColor color;
  for (int i = 0; i < fixedWidth; ++i) {
    for (int j = 0; j < fixedHeight; ++j) {
      uchar* src = bufferData + 4 * fixedWidth * j;
      color.setBlue((int)*(src + 0));
      color.setGreen((int)*(src + 1));
      color.setRed((int)*(src + 2));
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
  context = optix::Context::create();
  context->setRayTypeCount(2);
  context->setEntryPointCount(1);
  context->setStackSize(9608);

  context["maxDepth"]->setInt(128);
  context["rayTypeRadience"]->setUint(0);
  context["rayTypeShadow"]->setUint(1);
  context["rayEpsilonT"]->setFloat(1.e-4f);
  context["intensityCutOff"]->setFloat(1.e-2f);
  context["ambientLightColor"]->setFloat(0.31f, 0.31f, 0.31f);

  optix::Buffer outputBuffer = context->createBuffer(context, RT_FORMAT_UNSIGNED_BYTE4, fixedWidth, fixedHeight);
  context["outputBuffer"]->set(outputBuffer);

  // Exception
  optix::Program exptProgram = context->createProgramFromPTXFile("Exception.cu", "exception");
  context->setExceptionProgram(0, exptProgram);
  context["badColor"]->setFloat(1.f, 0.f, 0.f);

  // Miss
  optix::Program missProgram = context->createProgramFromPTXFile("MissProgram.cu", "staticMiss");
  context->setMissProgram(0, missProgram);
  context["bgColor"]->setFloat(0.34f, 0.55f, 0.85f);
}

void MinimalOptiX::setupScene(SceneNum num) {
  if (num == SCENE_0) {
    optix::Program sphereIntersect = context->createProgramFromPTXString("Geometry.cu", "sphereIntersect");
    optix::Program sphereBBox = context->createProgramFromPTXString("Geometry.cu", "sphereBBox");
    optix::Program staticMtl = context->createProgramFromPTXString("Material.cu", "closestHitStatic");

    optix::Geometry sphereMid = context->createGeometry();
    sphereMid->setPrimitiveCount(1u);
    sphereMid->setIntersectionProgram(sphereIntersect);
    sphereMid->setBoundingBoxProgram(sphereBBox);
    sphereMid["radius"]->setFloat(0.5f);
    sphereMid["center"]->setFloat(0.f, 0.f, -1.f);
    
    optix::Material sphereMidMtl = context->createMaterial();
    sphereMidMtl->setClosestHitProgram(0, staticMtl);
    sphereMidMtl["mtlColor"]->setFloat(0.1f, 0.2f, 0.5f);

    optix::GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    optix::GeometryGroup geoGrp = context->createGeometryGroup();
    geoGrp->setChildCount(1);
    geoGrp->setChild(0, sphereMidGI);
    geoGrp->setAcceleration(context->createAcceleration("NoAccel"));

    context["topObject"]->set(geoGrp);


    Camera camera;
    optix::float3 lookFrom = { 0.f, 1.5f, 1.5f };
    optix::float3 lookAt = { 0.f, 0.f, -1.f };
    optix::float3 up = { 0.f, 1.f, 0.f };
    camera.set(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight);
    optix::Program rayGenProgram = context->createProgramFromPTXFile("Camera.cu", "pinholeCamera");
    rayGenProgram["origin"]->setFloat(camera.origin);
    rayGenProgram["u"]->setFloat(camera.u);
    rayGenProgram["v"]->setFloat(camera.v);
    rayGenProgram["w"]->setFloat(camera.w);
    rayGenProgram["scrLowerLeftCorner"]->setFloat(camera.lowerLeftCorner);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
}
