#include "MinimalOptiX.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace optix;

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
  ui.view->setScene(&qgscene);

  canvas = QImage(ui.view->size(), QImage::Format_RGB888);

  compilePtx();
  setupContext();
  setupScene(scendId);
  context->validate();
  uint checkpoint = 1;
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
    if (isHDRendering) {
      if ((i + 1) % checkpoint == 0) {
        updateContent(i + 1, false);
        saveCurrentFrame(false);
        checkpoint *= 2;
      }
    }
  }
  updateContent(nSuperSampling, false);
  if (isHDRendering) {
    saveCurrentFrame(false);
  }
  //record(50, "sample_1.mpg");
}

void MinimalOptiX::compilePtx() {
  std::string value;

  for (auto& key : cuFiles) {
    cuFileToPtxStr(key, value);
    ptxStrs.insert(std::make_pair(key, value));
  }
}

void MinimalOptiX::updateContent(float nAccumulation, bool clearBuffer) {
  float* bufferData = (float*)context["accuBuffer"]->getBuffer()->map();
  QColor color;
  for (uint i = 0; i < fixedHeight; ++i) {
    for (uint j = 0; j < fixedWidth; ++j) {
      float* src = bufferData + 3 * (i * fixedWidth + j);
      color.setRedF(clamp(src[0] / nAccumulation, 0.f, 1.f));
      color.setGreenF(clamp(src[1] / nAccumulation, 0.f, 1.f));
      color.setBlueF(clamp(src[2] / nAccumulation, 0.f, 1.f));
      canvas.setPixelColor(j, fixedHeight - i - 1, color);
      if (clearBuffer) {
        src[0] = 0.0f;
        src[1] = 0.0f;
        src[2] = 0.0f;
      }
    }
  }
  context["accuBuffer"]->getBuffer()->unmap();

  QPixmap tmpPixmap = QPixmap::fromImage(canvas);
  qgscene.clear();
  qgscene.addPixmap(tmpPixmap);
  ui.view->update();
}

void MinimalOptiX::saveCurrentFrame(bool popUpDialog) {
  canvas.save(QString::number(QDateTime::currentMSecsSinceEpoch()) + QString(".png"));
  if (popUpDialog) {
    QMessageBox::information(
      this,
      "Save",
      "Image saved!",
      QMessageBox::Ok
    );
  }
}

void MinimalOptiX::keyPressEvent(QKeyEvent* e) {
  switch (e->key()) {
  case Qt::Key_Space:
    saveCurrentFrame(true);
    break;
  case Qt::Key_Return:
    animate(100);
    render(scendId);
    break;
  case Qt::Key_W:
    lookFrom += {0.0f, 0.0f, 1.0f};
    render(scendId);
    break;
  case Qt::Key_S:
    lookFrom -= {0.0f, 0.0f, 1.0f};
    render(scendId);
    break;
  case Qt::Key_A:
    lookFrom += {1.0f, 0.0f, 0.0f};
    render(scendId);
    break;
  case Qt::Key_D:
    lookFrom -= {1.0f, 0.0f, 0.0f};
    render(scendId);
    break;
  }
}

void MinimalOptiX::setupContext() {
  context = Context::create();
  context->setRayTypeCount(2);
  context->setEntryPointCount(1);
  context->setStackSize(9608);

  context["rayTypeRadiance"]->setUint(RAY_TYPE_RADIANCE);
  context["rayTypeShadow"]->setUint(RAY_TYPE_SHADOW);
  context["rayMaxDepth"]->setUint(rayMaxDepth);
  context["rayMinIntensity"]->setFloat(rayMinIntensity);
  context["rayEpsilonT"]->setFloat(rayEpsilonT);
  context["absorbColor"]->setFloat(0.f, 0.f, 0.f);
  context["nSuperSampling"]->setUint(nSuperSampling);

  Buffer accuBuffer = context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT3, fixedWidth, fixedHeight);
  memset((float*)accuBuffer->map(), 0, sizeof(float) * 3 * fixedWidth * fixedHeight);
  accuBuffer->unmap();
  context["accuBuffer"]->set(accuBuffer);

  // Exception
  Program exptProgram = context->createProgramFromPTXString(ptxStrs[exCuFileName], "exception");
  context->setExceptionProgram(0, exptProgram);
  context["badColor"]->setFloat(0.f, 0.f, 0.f);

  // Miss
  Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
  context->setMissProgram(0, missProgram);
  //missProgram["bgColor"]->setFloat(1.f, 1.f, 1.f);
  missProgram["bgColor"]->setFloat(0.5f, 0.5f, 0.5f);
}

void MinimalOptiX::setupScene(SceneId sceneId) {
  if (sceneId == SCENE_BASIC_TEST) {
    // objects
    Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
    Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
    Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
    Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
    Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
    Program metalMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "metal");
    Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
    Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");

    Geometry sphereMid = context->createGeometry();
    sphereMid->setPrimitiveCount(1u);
    sphereMid->setIntersectionProgram(sphereIntersect);
    sphereMid->setBoundingBoxProgram(sphereBBox);

    sphereMid["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams);
    Material sphereMidMtl = context->createMaterial();

    sphereMidMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lambMtl);
    LambertianParams lambParams = { {0.1f, 0.2f, 0.5f}, defaultNScatter, 1 };
    sphereMidMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    Geometry sphereRight = context->createGeometry();
    sphereRight->setPrimitiveCount(1u);
    sphereRight->setIntersectionProgram(sphereIntersect);
    sphereRight->setBoundingBoxProgram(sphereBBox);
    sphereRight["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams + 1);
    Material sphereRightMtl = context->createMaterial();
    sphereRightMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, metalMtl);
    MetalParams metalParams = { {0.8f, 0.6f, 0.2f}, 0.f };
    sphereRightMtl["metalParams"]->setUserData(sizeof(MetalParams), &metalParams);
    GeometryInstance sphereRightGI = context->createGeometryInstance(sphereRight, &sphereRightMtl, &sphereRightMtl + 1);

    Geometry sphereLeft = context->createGeometry();
    sphereLeft->setPrimitiveCount(1u);
    sphereLeft->setIntersectionProgram(sphereIntersect);
    sphereLeft->setBoundingBoxProgram(sphereBBox);
    sphereLeft["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams + 2);
    Material sphereLeftMtl = context->createMaterial();
    sphereLeftMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, glassMtl);
    GlassParams glassParams = { {1.f, 1.f, 1.f}, 1.5f };
    sphereLeftMtl["glassParams"]->setUserData(sizeof(glassParams), &glassParams);
    GeometryInstance sphereLeftGI = context->createGeometryInstance(sphereLeft, &sphereLeftMtl, &sphereLeftMtl + 1);

    Geometry quadFloor = context->createGeometry();
    quadFloor->setPrimitiveCount(1u);
    quadFloor->setIntersectionProgram(quadIntersect);
    quadFloor->setBoundingBoxProgram(quadBBox);
    QuadParams quadParams;
    float3 anchor = { -2.f, -0.5f, -0.5f };
    float3 v1 = { 0.f, 0.f, -2.f };
    float3 v2 = { 4.f, 0.f, 0.f };
    setQuadParams(anchor, v1, v2, quadParams);
    quadFloor["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
    Material quadFloorMtl = context->createMaterial();
    quadFloorMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lambMtl);
    lambParams.albedo = make_float3(0.8f, 0.8f, 0.f);
    lambParams.scatterMaxDepth = 1;
    quadFloorMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    GeometryInstance quadFloorGI = context->createGeometryInstance(quadFloor, &quadFloorMtl, &quadFloorMtl + 1);

    Geometry quadLight = context->createGeometry();
    quadLight->setPrimitiveCount(1u);
    quadLight->setIntersectionProgram(quadIntersect);
    quadLight->setBoundingBoxProgram(quadBBox);
    anchor = { -5.f, 5.f, 5.f };
    v1 = { 0.f, 0.f, -10.f };
    v2 = { 10.f, 0.f, 0.f };
    setQuadParams(anchor, v1, v2, quadParams);
    quadLight["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
    Material quadLightMtl = context->createMaterial();
    quadLightMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lightMtl);
    LightParams lightParams;
    lightParams.emission = make_float3(1.f);
    quadLightMtl["lightParams"]->setUserData(sizeof(LightParams), &lightParams);
    GeometryInstance quadLightGI = context->createGeometryInstance(quadLight, &quadLightMtl, &quadLightMtl + 1);

    std::vector<GeometryInstance> objs = { sphereMidGI, quadFloorGI, quadLightGI, sphereRightGI, sphereLeftGI };
    GeometryGroup geoGrp = context->createGeometryGroup();
    geoGrp->setChildCount(uint(objs.size()));
    for (auto i = 0; i < objs.size(); ++i) {
      geoGrp->setChild(i, objs[i]);
    }
    geoGrp->setAcceleration(context->createAcceleration("NoAccel"));
    context["topGroup"]->set(geoGrp);

    //// camera
    //CamParams camParams;
    //float3 lookFrom = { 0.f, 1.f, 2.f };
    //float3 lookAt = { 0.f, 0.f, -1.f };
    //float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
    // ======================================================================================================================
  }
  else if (sceneId == SCENE_MESH_TEST) {
    // ======================================================================================================================
    // objects
    Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
    Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
    Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
    Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
    Program meshIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshIntersect");
    Program meshBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshBBox");
    Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
    Program metalMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "metal");
    Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
    Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");
    Program disneyMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disney");

    Geometry geo = context->createGeometry();
    geo->setPrimitiveCount(4u);
    geo->setIntersectionProgram(meshIntersect);
    geo->setBoundingBoxProgram(meshBBox);

    Buffer vertexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, 4u);
    float vertBufSrc[] = {
      0.f, 0.f, -0.75f,
      0.5f, 0.f, 0.25f,
      -0.5f, 0.f, 0.25f,
      0.f, 1.f, 0.f
    };
    memcpy(vertexBuffer->map(), vertBufSrc, sizeof(float) * 12);
    vertexBuffer->unmap();
    geo["vertexBuffer"]->set(vertexBuffer);

    Buffer normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, 0);
    geo["normalBuffer"]->set(normalBuffer);
    Buffer texcoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, 0);
    geo["texcoordBuffer"]->set(texcoordBuffer);


    Buffer indexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, 4u);
    int idxBufSrc[] = {
      1, 0, 3,
      1, 3, 2,
      3, 0, 2,
      0, 1, 2
    };
    memcpy(indexBuffer->map(), idxBufSrc, sizeof(int) * 12);
    indexBuffer->unmap();
    geo["indexBuffer"]->set(indexBuffer);

    Material mtl = context->createMaterial();
    mtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lambMtl);
    LambertianParams lambParams = { { 0.3f, 0.5f, 0.7f }, 16, 1 };
    mtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);

    GeometryInstance gi = context->createGeometryInstance(geo, &mtl, &mtl + 1);

    GeometryGroup grp = context->createGeometryGroup();
    grp->setChildCount(1u);
    grp->setChild(0, gi);
    grp->setAcceleration(context->createAcceleration("NoAccel"));
    context["topGroup"]->set(grp);

    // Camera
    CamParams camParams;
    auto lookFrom = make_float3(0.f, 0.3f, -2.f);
    auto lookAt = make_float3(0.f, 0.3f, 0.f);
    auto up = make_float3(0.f, 1.f, 0.f);
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_COFFEE) {
    setupScene("coffee");
    CamParams camParams;
    float3 lookFrom = make_float3(0.f, 0.22 * aabb.extent(1), 0.25 * aabb.extent(2));
    float3 lookAt = lookFrom + make_float3(0.f, -0.01875f, -1.f);
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_BEDROOM) {
    setupScene("bedroom");
    CamParams camParams;
    float3 lookFrom = aabb.center() + make_float3(0.3f, 0.1f, 0.45f) * aabb.extent();
    float3 lookAt = aabb.center() + make_float3(0.05f, -0.1f, 0.f) * aabb.extent();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_DININGROOM) {
    setupScene("diningroom");
    CamParams camParams;
    float3 lookFrom = aabb.center() + make_float3(-0.7f, 0.f, 0.f) * aabb.extent();
    float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_STORMTROOPER) {
    setupScene("stormtrooper");
    CamParams camParams;
    float3 lookFrom = aabb.center() + make_float3(0.25f, 0.1f, 0.395f) * aabb.extent();
    float3 lookAt = aabb.center() + make_float3(0.25f, 0.1f, 0.f) * aabb.extent();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 30, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_SPACESHIP) {
    setupScene("spaceship");
    CamParams camParams;
    float3 lookFrom = aabb.center() + make_float3(-0.03f, 0.03f, -0.03f) * aabb.extent();
    float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_CORNELL) {
    setupScene("cornell");
    CamParams camParams;
    float3 lookFrom = aabb.center() + make_float3(0.f, 0.f, -2.f) * aabb.extent();
    float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 39.3077, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
  else if (sceneId == SCENE_HYPERION || sceneId == SCENE_DRAGON) {
    setupScene("hyperion");
    CamParams camParams;
    float3 lookFrom;
    if (sceneId == SCENE_HYPERION) {
      lookFrom = aabb.center() + make_float3(-0.08f, 2.f, 0.f) * aabb.extent();
    } else {
      lookFrom = aabb.center() + make_float3(0.05f, 0.3f, -0.005f) * aabb.extent();
    }
    float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 30, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }
}

void MinimalOptiX::setupScene(const char* sceneName) {
  Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
  Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
  Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
  Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
  Program meshIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshIntersect");
  Program meshBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshBBox");
  Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
  Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");
  Program disneyMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disney");
  Program disneyAnyHit = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disneyAnyHit");

  std::string sceneFolder = baseSceneFolder + sceneName + "/";
  Scene scene((sceneFolder + sceneName + ".scene").c_str());
  std::map<std::string, TextureSampler> texNameSamplerMap;

  GeometryGroup meshGroup = context->createGeometryGroup();
  meshGroup->setAcceleration(context->createAcceleration("Trbvh"));
  for (int i = 0; i < scene.meshNames.size(); ++i) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, (sceneFolder + scene.meshNames[i]).c_str());
    if (!err.empty() || !ret) {
      std::cerr << err << std::endl;
      throw std::logic_error("Cannot load mesh file.");
    }
    for (size_t s = 0; s < shapes.size(); s++) {
      // geometry
      Geometry geo = context->createGeometry();
      geo->setPrimitiveCount(uint(shapes[s].mesh.num_face_vertices.size()));
      geo->setIntersectionProgram(meshIntersect);
      geo->setBoundingBoxProgram(meshBBox);

      Buffer vertexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, attrib.vertices.size() / 3);
      memcpy(vertexBuffer->map(), attrib.vertices.data(), sizeof(float) * attrib.vertices.size());
      vertexBuffer->unmap();
      geo["vertexBuffer"]->set(vertexBuffer);

      Buffer normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, attrib.normals.size() / 3);
      if (!attrib.normals.empty()) {
        memcpy(normalBuffer->map(), attrib.normals.data(), sizeof(float) * attrib.normals.size());
        normalBuffer->unmap();
      }
      geo["normalBuffer"]->set(normalBuffer);

      Buffer texcoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, attrib.texcoords.size() / 2);
      if (!attrib.texcoords.empty()) {
        memcpy(texcoordBuffer->map(), attrib.texcoords.data(), sizeof(float) * attrib.texcoords.size());
        texcoordBuffer->unmap();
      }
      geo["texcoordBuffer"]->set(texcoordBuffer);


      Buffer vertIdxBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, shapes[s].mesh.num_face_vertices.size());
      int* vertIdxBufDst = (int*)vertIdxBuffer->map();
      Buffer texIdxBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, shapes[s].mesh.num_face_vertices.size());
      int* texIdxBufDst = (int*)texIdxBuffer->map();
      Buffer normIdxBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, shapes[s].mesh.num_face_vertices.size());
      int* normIdxBufDst = (int*)normIdxBuffer->map();
      for (int f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
        for (int fv = 0; fv < 3; ++fv) {
          auto& idx = shapes[s].mesh.indices[f * 3 + fv];
          vertIdxBufDst[f * 3 + fv] = idx.vertex_index;
          texIdxBufDst[f * 3 + fv] = idx.texcoord_index;
          normIdxBufDst[f * 3 + fv] = idx.normal_index;
          tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
          tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
          tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
          aabb.include(make_float3(vx, vy, vz));
        }
      }
      vertIdxBuffer->unmap();
      texIdxBuffer->unmap();
      normIdxBuffer->unmap();
      geo["vertIdxBuffer"]->set(vertIdxBuffer);
      geo["texIdxBuffer"]->set(texIdxBuffer);
      geo["normIdxBuffer"]->set(normIdxBuffer);

      // texture
      if (!scene.textures[i].empty()) {
        if (texNameSamplerMap.find(scene.textures[i]) == texNameSamplerMap.end()) {
          QImage img((sceneFolder + scene.textures[i]).c_str());

          TextureSampler sampler = context->createTextureSampler();
          sampler->setWrapMode(0, RT_WRAP_REPEAT);
          sampler->setWrapMode(1, RT_WRAP_REPEAT);
          sampler->setWrapMode(2, RT_WRAP_REPEAT);
          sampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
          sampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
          sampler->setMaxAnisotropy(1.f);
          sampler->setMipLevelCount(1u);
          sampler->setArraySize(1u);

          Buffer buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, img.width(), img.height());
          float* bufferData = (float*)buffer->map();
          for (int i = 0; i < img.width(); ++i) {
            for (int j = 0; j < img.height(); ++j) {
              float* dst = bufferData + 4 * (j * img.width() + i);
              auto color = img.pixelColor(i, img.height() - j - 1);
              dst[0] = color.redF();
              dst[1] = color.greenF();
              dst[2] = color.blueF();
              dst[3] = 1.f;
            }
          }
          buffer->unmap();

          sampler->setBuffer(0u, 0u, buffer);
          sampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);

          texNameSamplerMap[scene.textures[i]] = sampler;
        }
        scene.materials[i].albedoID = texNameSamplerMap[scene.textures[i]]->getId();
      }

      // material
      Material mtl = context->createMaterial();
      mtl->setClosestHitProgram(RAY_TYPE_RADIANCE, disneyMtl);
      mtl->setAnyHitProgram(RAY_TYPE_SHADOW, disneyAnyHit);
      mtl["disneyParams"]->setUserData(sizeof(DisneyParams), &(scene.materials[i]));

      GeometryInstance meshGI = context->createGeometryInstance(geo, &mtl, &mtl + 1);
      meshGroup->addChild(meshGI);
    }
  }

  // lights
  GeometryGroup lightGroup = context->createGeometryGroup();
  lightGroup->setAcceleration(context->createAcceleration("Trbvh"));
  for (auto& light : scene.lights) {
    Geometry geo = context->createGeometry();
    geo->setPrimitiveCount(1u);
    if (light.shape == SPHERE) {
      geo->setIntersectionProgram(sphereIntersect);
      geo->setBoundingBoxProgram(sphereBBox);
      SphereParams params;
      params.radius = light.radius;
      params.center = light.position;
      geo["sphereParams"]->setUserData(sizeof(SphereParams), &params);
    }
    else if (light.shape == QUAD) {
      geo->setIntersectionProgram(quadIntersect);
      geo->setBoundingBoxProgram(quadBBox);
      QuadParams params;
      setQuadParams(light.position, light.u, light.v, params);
      geo["quadParams"]->setUserData(sizeof(QuadParams), &params);
    }
    else {
      throw std::logic_error("No shape for light.");
    }

    Material mtl = context->createMaterial();
    mtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lightMtl);
    mtl["lightParams"]->setUserData(sizeof(LightParams), &light);

    GeometryInstance gi = context->createGeometryInstance(geo, &mtl, &mtl + 1);
    lightGroup->addChild(gi);
  }

  Buffer lightBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER);
  lightBuffer->setElementSize(sizeof(LightParams));
  lightBuffer->setSize(scene.lights.size());
  LightParams* lightBufData = (LightParams*)lightBuffer->map();
  for (int i = 0; i < scene.lights.size(); ++i) {
    memcpy(lightBufData + i, &(scene.lights[i]), sizeof(LightParams));
  }
  lightBuffer->unmap();
  context["lights"]->setBuffer(lightBuffer);

  Group topGroup = context->createGroup();
  topGroup->setAcceleration(context->createAcceleration("Trbvh"));
  topGroup->addChild(meshGroup);
  topGroup->addChild(lightGroup);
  context["topGroup"]->set(topGroup);
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

void MinimalOptiX::render(SceneId sceneId) {
  setupScene(sceneId);
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
  }
  updateContent(nSuperSampling, true);
}

void MinimalOptiX::record(int frames, const char* filename) {
  std::vector<QImage> images;
  for (int i = 0; i < frames; ++i) {
    animate(10);
    render(scendId);
    //if (i % 10 == 0)
    //  canvas.save(QString::number(i / 10) + QString(".png"));
    images.push_back(canvas);
  }
  generateVideo(images, filename);
}
