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
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
  }
  update(ACCU_BUFFER);
  //record(750);
}

void MinimalOptiX::compilePtx() {
  std::string value;

  for (auto& key : cuFiles) {
    cuFileToPtxStr(key, value);
    ptxStrs.insert(std::make_pair(key, value));
  }
}

void MinimalOptiX::update(UpdateSource source) {
  if (source == OUTPUT_BUFFER) {
    uchar* bufferData = (uchar*)context["outputBuffer"]->getBuffer()->map();
    QColor color;
    for (uint i = 0; i < fixedHeight; ++i) {
      for (uint j = 0; j < fixedWidth; ++j) {
        uchar* src = bufferData + 4 * (i * fixedWidth + j);
        color.setRed((int)*(src + 0));
        color.setGreen((int)*(src + 1));
        color.setBlue((int)*(src + 2));
        canvas.setPixelColor(j, fixedHeight - i - 1, color);
      }
    }
    context["outputBuffer"]->getBuffer()->unmap();
  } else if (source == ACCU_BUFFER) {
    float* bufferData = (float*)context["accuBuffer"]->getBuffer()->map();
    QColor color;
    for (uint i = 0; i < fixedHeight; ++i) {
      for (uint j = 0; j < fixedWidth; ++j) {
        float* src = bufferData + 3 * (i * fixedWidth + j);
        color.setRedF(src[0] / (float)nSuperSampling);
        color.setGreenF(src[1] / (float)nSuperSampling);
        color.setBlueF(src[2] / (float)nSuperSampling);
        canvas.setPixelColor(j, fixedHeight - i - 1, color);
        src[0] = 0.0f;
        src[1] = 0.0f;
        src[2] = 0.0f;
      }
    }
    context["accuBuffer"]->getBuffer()->unmap();
  }

  QPixmap tmpPixmap = QPixmap::fromImage(canvas);
  scene.clear();
  scene.addPixmap(tmpPixmap);
  ui.view->update();
}

void MinimalOptiX::keyPressEvent(QKeyEvent* e) {
  switch (e->key()) {
  case Qt::Key_Space:
    canvas.save(QString::number(QDateTime::currentMSecsSinceEpoch()) + QString(".png"));
    QMessageBox::information(
      this,
      "Save",
      "Image saved!",
      QMessageBox::Ok
    );
    break;
  case Qt::Key_Return:
    animate(100);
    render();
    break;  
  case Qt::Key_W:
    lookFrom += {0.0f, 0.0f, 1.0f};
    render();
    break;
  case Qt::Key_S:
    lookFrom -= {0.0f, 0.0f, 1.0f};
    render();
    break;
  case Qt::Key_A:
    lookFrom += {1.0f, 0.0f, 0.0f};
    render();
    break;
  case Qt::Key_D:
    lookFrom -= {1.0f, 0.0f, 0.0f};
    render();
    break;
  }
}

void MinimalOptiX::setupContext() {
  context = optix::Context::create();
  context->setRayTypeCount(1);
  context->setEntryPointCount(1);
  context->setStackSize(9608);

  context["rayTypeRadiance"]->setUint(0);
  context["rayMaxDepth"]->setUint(rayMaxDepth);
  context["rayMinIntensity"]->setFloat(rayMinIntensity);
  context["rayEpsilonT"]->setFloat(rayEpsilonT);
  context["absorbColor"]->setFloat(0.f, 0.f, 0.f);
  context["nSuperSampling"]->setUint(nSuperSampling);


  optix::Buffer outputBuffer = context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_UNSIGNED_BYTE4, fixedWidth, fixedHeight);
  context["outputBuffer"]->set(outputBuffer);
  optix::Buffer accuBuffer = context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT3, fixedWidth, fixedHeight);
  context["accuBuffer"]->set(accuBuffer);

  // Exception
  optix::Program exptProgram = context->createProgramFromPTXString(ptxStrs[exCuFileName], "exception");
  context->setExceptionProgram(0, exptProgram);
  context["badColor"]->setFloat(1.f, 0.f, 0.f);

  // Miss
  optix::Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
  context->setMissProgram(0, missProgram);
  missProgram["bgColor"]->setFloat(0.3f, 0.3f, 0.3f);
}

void MinimalOptiX::setupScene(SceneNum num) {
  if (num == SCENE_0) {
    // objects
    optix::Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
    optix::Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
    optix::Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
    optix::Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
    optix::Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
    optix::Program metalMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "metal");
    optix::Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
    optix::Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");

    optix::Geometry sphereMid = context->createGeometry();
    sphereMid->setPrimitiveCount(1u);
    sphereMid->setIntersectionProgram(sphereIntersect);
    sphereMid->setBoundingBoxProgram(sphereBBox);

    sphereMid["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams);
    optix::Material sphereMidMtl = context->createMaterial();
    sphereMidMtl->setClosestHitProgram(0, lambMtl);
    LambertianParams lambParams = { {0.1f, 0.2f, 0.5f}, defaultNScatter, 1 };
    sphereMidMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    optix::GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    optix::Geometry sphereRight = context->createGeometry();
    sphereRight->setPrimitiveCount(1u);
    sphereRight->setIntersectionProgram(sphereIntersect);
    sphereRight->setBoundingBoxProgram(sphereBBox);
    sphereRight["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams + 1);
    optix::Material sphereRightMtl = context->createMaterial();
    sphereRightMtl->setClosestHitProgram(0, metalMtl);
    MetalParams metalParams = { {0.8f, 0.6f, 0.2f}, 0.f };
    sphereRightMtl["metalParams"]->setUserData(sizeof(MetalParams), &metalParams);
    optix::GeometryInstance sphereRightGI = context->createGeometryInstance(sphereRight, &sphereRightMtl, &sphereRightMtl + 1);

    optix::Geometry sphereLeft = context->createGeometry();
    sphereLeft->setPrimitiveCount(1u);
    sphereLeft->setIntersectionProgram(sphereIntersect);
    sphereLeft->setBoundingBoxProgram(sphereBBox);
    sphereLeft["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams + 2);
    optix::Material sphereLeftMtl = context->createMaterial();
    sphereLeftMtl->setClosestHitProgram(0, glassMtl);
    GlassParams glassParams = { {1.f, 1.f, 1.f}, 1.5f, defaultNScatter, 1 };
    sphereLeftMtl["glassParams"]->setUserData(sizeof(glassParams), &glassParams);
    optix::GeometryInstance sphereLeftGI = context->createGeometryInstance(sphereLeft, &sphereLeftMtl, &sphereLeftMtl + 1);

    optix::Geometry quadFloor = context->createGeometry();
    quadFloor->setPrimitiveCount(1u);
    quadFloor->setIntersectionProgram(quadIntersect);
    quadFloor->setBoundingBoxProgram(quadBBox);
    QuadParams quadParams;
    optix::float3 anchor = { -2.f, -0.5f, -0.5f };
    optix::float3 v1 = { 0.f, 0.f, -2.f };
    optix::float3 v2 = { 4.f, 0.f, 0.f };
    setQuadParams(anchor, v1, v2, quadParams);
    quadFloor["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
    optix::Material quadFloorMtl = context->createMaterial();
    quadFloorMtl->setClosestHitProgram(0, lambMtl);
    lambParams.albedo = optix::make_float3(0.8f, 0.8f, 0.f);
    lambParams.scatterMaxDepth = 1;
    quadFloorMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    optix::GeometryInstance quadFloorGI = context->createGeometryInstance(quadFloor, &quadFloorMtl, &quadFloorMtl + 1);

    optix::Geometry quadLight = context->createGeometry();
    quadLight->setPrimitiveCount(1u);
    quadLight->setIntersectionProgram(quadIntersect);
    quadLight->setBoundingBoxProgram(quadBBox);
    anchor = { -5.f, 5.f, 5.f };
    v1 = { 0.f, 0.f, -10.f };
    v2 = { 10.f, 0.f, 0.f };
    setQuadParams(anchor, v1, v2, quadParams);
    quadLight["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
    optix::Material quadLightMtl = context->createMaterial();
    quadLightMtl->setClosestHitProgram(0, lightMtl);
    quadLightMtl["lightColor"]->setFloat(1.f, 1.f, 1.f);
    optix::GeometryInstance quadLightGI = context->createGeometryInstance(quadLight, &quadLightMtl, &quadLightMtl + 1);

    std::vector<optix::GeometryInstance> objs = { sphereMidGI, quadFloorGI, quadLightGI, sphereRightGI, sphereLeftGI };
    optix::GeometryGroup geoGrp = context->createGeometryGroup();
    geoGrp->setChildCount(uint(objs.size()));
    for (auto i = 0; i < objs.size(); ++i) {
      geoGrp->setChild(i, objs[i]);
    }
    geoGrp->setAcceleration(context->createAcceleration("NoAccel"));
    context["topObject"]->set(geoGrp);

    //// camera
    //CamParams camParams;
    //optix::float3 lookFrom = { 0.f, 1.f, 2.f };
    //optix::float3 lookAt = { 0.f, 0.f, -1.f };
    //optix::float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    optix::Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
}
void MinimalOptiX::move(SphereParams& param, float time) {
  float distance = param.velocity.y * time + time * time * gravity / 2.0f;
  if (distance < param.center.y - param.radius + 0.5f) { // -0.5f is the plane
    param.center.x += param.velocity.x * time;
    param.center.z += param.velocity.z * time;
    param.center.y -= distance;
    param.velocity.y += gravity * time;
  } else {
    float vend = sqrt(param.velocity.y * param.velocity.y + (2.0f * gravity * (param.center.y - param.radius + 0.5f)));
    float t = (vend - param.velocity.y) / gravity;
    if (t < 1e-6) {
      param.velocity.y = 0.f;
      param.center.y = -0.5f + param.radius;
      return;
    }
    param.center.x += param.velocity.x * t;
    param.center.z += param.velocity.z * t;
    param.center.y = -0.5f + param.radius;
    param.velocity.x *= attenuationCoef;
    param.velocity.y *= attenuationCoef;
    param.velocity.y = -vend * attenuationCoef;
    move(param, time - t);
  }
}
void MinimalOptiX::animate(int ticks) {
  for (int i = 0; i < 3; ++i) {
    move(sphereParams[i], ticks / 1000.f);
  }
}

void MinimalOptiX::render() {
  setupScene(SCENE_0);
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
  }
  update(ACCU_BUFFER);
}

void MinimalOptiX::record(int frames) {
  std::vector<QImage> images;
  for (int i = 0; i < frames; ++i) {
    animate(10);
    render();
    if (i % 10 == 0)
      canvas.save(QString::number(i / 10) + QString(".png"));
    images.push_back(canvas);
  }
  generateVideo(images, "test2.mpg");
}