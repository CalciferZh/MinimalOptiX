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

  compilePtx();
  setupContext();
  setupScene(SCENE_0);
  context->validate();
  context->launch(0, fixedWidth, fixedHeight);
  update();
}

void MinimalOptiX::compilePtx() {
  std::string value;

  for (auto& key : cuFiles) {
    cuFileToPtxStr(key, value);
    ptxStrs.insert(std::make_pair(key, value));
  }
}

void MinimalOptiX::update() {
  uchar* bufferData = (uchar*)context["outputBuffer"]->getBuffer()->map();

  QColor color;
  for (uint i = 0; i < fixedHeight; ++i) {
    for (uint j = 0; j < fixedWidth; ++j) {
      uchar* src = bufferData + 4 * (i * fixedWidth + j);
      color.setBlue((int)*(src + 0));
      color.setGreen((int)*(src + 1));
      color.setRed((int)*(src + 2));
      canvas.setPixelColor(j, fixedHeight - i - 1, color);
    }
  }

  context["outputBuffer"]->getBuffer()->unmap();

  QPixmap tmpPixmap = QPixmap::fromImage(canvas);
  scene.clear();
  scene.addPixmap(tmpPixmap);
  ui.view->update();
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
  context->setRayTypeCount(1);
  context->setEntryPointCount(1);
  context->setStackSize(9608);

  context["rayTypeRadience"]->setUint(0);
  context["rayMaxDepth"]->setUint(rayMaxDepth);
  context["rayMinIntensity"]->setFloat(rayMinIntensity);
  context["rayEpsilonT"]->setFloat(rayEpsilonT);


  optix::Buffer outputBuffer = context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_UNSIGNED_BYTE4, fixedWidth, fixedHeight);
  context["outputBuffer"]->set(outputBuffer);

  // Exception
  optix::Program exptProgram = context->createProgramFromPTXString(ptxStrs[exCuFileName], "exception");
  context->setExceptionProgram(0, exptProgram);
  context["badColor"]->setFloat(1.f, 0.f, 0.f);

  // Miss
  optix::Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "vGradMiss");
  context->setMissProgram(0, missProgram);
  context["gradColorMax"]->setFloat(0.5f, 0.7f, 1.f);
  context["gradColorMin"]->setFloat(1.f, 1.f, 1.f);
}

void MinimalOptiX::setupScene(SceneNum num) {
  if (num == SCENE_0) {
    // objects
    optix::Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
    optix::Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
    optix::Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
    optix::Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
    optix::Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
    optix::Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");

    optix::Geometry sphereMid = context->createGeometry();
    sphereMid->setPrimitiveCount(1u);
    sphereMid->setIntersectionProgram(sphereIntersect);
    sphereMid->setBoundingBoxProgram(sphereBBox);
    sphereMid["radius"]->setFloat(0.5f);
    sphereMid["center"]->setFloat(0.f, 0.f, -1.f);
    optix::Material sphereMidMtl = context->createMaterial();
    sphereMidMtl->setClosestHitProgram(0, lambMtl);
    sphereMidMtl["mtlColor"]->setFloat(0.1f, 0.2f, 0.5f);
    sphereMidMtl["nScatter"]->setFloat(4.f);
    optix::GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    optix::Geometry quadFloor = context->createGeometry();
    quadFloor->setPrimitiveCount(1u);
    quadFloor->setIntersectionProgram(quadIntersect);
    quadFloor->setBoundingBoxProgram(quadBBox);
    optix::float3 anchor = { -100.f, -0.5f, 100.f };
    optix::float3 v1 = { 0.f, 0.f, -200.f };
    optix::float3 v2 = { 200.f, 0.f, 0.f };
    optix::float3 normal = normalize(optix::cross(v2, v1));
    float d = dot(normal, anchor);
    v1 /= dot(v1, v1);
    v2 /= dot(v2, v2);
    optix::float4 plane = make_float4(normal, d);
    quadFloor["v1"]->setFloat(v1);
    quadFloor["v2"]->setFloat(v2);
    quadFloor["plane"]->setFloat(plane);
    quadFloor["anchor"]->setFloat(anchor);
    optix::Material quadFloorMtl = context->createMaterial();
    quadFloorMtl->setClosestHitProgram(0, lambMtl);
    quadFloorMtl["mtlColor"]->setFloat(0.8f, 0.8f, 0.f);
    sphereMidMtl["nScatter"]->setFloat(4.f);
    optix::GeometryInstance quadFloorGI = context->createGeometryInstance(quadFloor, &quadFloorMtl, &quadFloorMtl + 1);

    optix::Geometry quadLight = context->createGeometry();
    quadLight->setPrimitiveCount(1u);
    quadLight->setIntersectionProgram(quadIntersect);
    quadLight->setBoundingBoxProgram(quadBBox);
    anchor = { -5.f, 5.f, 5.f };
    v1 = { 0.f, 0.f, -10.f };
    v2 = { 10.f, 0.f, 0.f };
    normal = normalize(optix::cross(v2, v1));
    d = dot(normal, anchor);
    v1 /= dot(v1, v1);
    v2 /= dot(v2, v2);
    plane = make_float4(normal, d);
    quadLight["v1"]->setFloat(v1);
    quadLight["v2"]->setFloat(v2);
    quadLight["plane"]->setFloat(plane);
    quadLight["anchor"]->setFloat(anchor);
    optix::Material quadLightMtl = context->createMaterial();
    quadLightMtl->setClosestHitProgram(0, lightMtl);
    quadLightMtl["lightColor"]->setFloat(1.f, 1.f, 1.f);
    optix::GeometryInstance quadLightGI = context->createGeometryInstance(quadLight, &quadLightMtl, &quadLightMtl + 1);

    std::vector<optix::GeometryInstance> objs = { sphereMidGI, quadFloorGI, quadLightGI };
    optix::GeometryGroup geoGrp = context->createGeometryGroup();
    geoGrp->setChildCount(uint(objs.size()));
    for (auto i = 0; i < objs.size(); ++i) {
      geoGrp->setChild(i, objs[i]);
    }
    geoGrp->setAcceleration(context->createAcceleration("NoAccel"));
    context["topObject"]->set(geoGrp);

    // camera
    Camera camera;
    optix::float3 lookFrom = { 0.f, 0.f, 1.f };
    optix::float3 lookAt = { 0.f, 0.f, -1.f };
    optix::float3 up = { 0.f, 1.f, 0.f };
    camera.set(lookFrom, lookAt, up, 60, (float)fixedWidth / (float)fixedHeight);
    optix::Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["origin"]->setFloat(camera.origin);
    rayGenProgram["horizontal"]->setFloat(camera.horizontal);
    rayGenProgram["vertical"]->setFloat(camera.vertical);
    rayGenProgram["scrLowerLeftCorner"]->setFloat(camera.lowerLeftCorner);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
}
