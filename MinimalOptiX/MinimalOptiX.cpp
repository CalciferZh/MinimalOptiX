#include <random>
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

  //sceneId = SCENE_SPHERES;
  //renderScene();

  //imageDemo();
  videoDemo();
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

void MinimalOptiX::saveCurrentFrame(bool popUpDialog, std::string fileNamePrefix) {
  QString fileName;
  if (fileNamePrefix.empty()) {
    fileName = QString::number(QDateTime::currentMSecsSinceEpoch()) + QString(".png");
  } else {
    fileName = QString::fromStdString(fileNamePrefix) + QString(".png");
  }
  canvas.save(fileName);
  if (popUpDialog) {
    QMessageBox::information(
      this,
      "Save",
      "Image saved to " + fileName,
      QMessageBox::Ok
    );
  }
}

void MinimalOptiX::imageDemo() {
  nSuperSampling = 4096u;
  sceneId = SCENE_COFFEE;
  renderScene(true, "coffee");
  sceneId = SCENE_BEDROOM;
  renderScene(true, "bedroom");
  sceneId = SCENE_DININGROOM;
  renderScene(true, "diningroom");
  sceneId = SCENE_STORMTROOPER;
  renderScene(true, "stormtrooper");
  sceneId = SCENE_SPACESHIP;
  renderScene(true, "spaceship");
  sceneId = SCENE_CORNELL;
  renderScene(true, "cornell");
  sceneId = SCENE_HYPERION;
  renderScene(true, "hyperion");
  sceneId = SCENE_DRAGON;
  renderScene(true, "dragon");
  QMessageBox::information(
    this,
    "Done",
    "All images have been saved.",
    QMessageBox::Ok
  );
}

void MinimalOptiX::videoDemo() {
  nSuperSampling = 64u;
  sceneId = SCENE_SPHERES_VIDEO;
  renderScene(false, "VIDEO");
  record(1000, "test.mp4", true);
}

void MinimalOptiX::keyPressEvent(QKeyEvent* e) {
  switch (e->key()) {
  case Qt::Key_Space:
    saveCurrentFrame(true);
    break;
  case Qt::Key_Return:
    updateVideo();
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

  Program exptProgram = context->createProgramFromPTXString(ptxStrs[exCuFileName], "exception");
  context->setExceptionProgram(0, exptProgram);
  context["badColor"]->setFloat(1.f, 1.f, 1.f);
}

void MinimalOptiX::setupScene() {
  aabb.invalidate();
  if (sceneId == SCENE_SPHERES) {
    SphereParams sphereParams[3] = {
      { 0.5f,{ 0.f, 0.f, -1.f },{ 0.f, 0.f, 0.f } } ,
    { 0.5f,{ 1.f, 0.f, -1.f },{ 0.f, 0.5f, 0.f } } ,
    { 0.5f,{ -1.f, 0.f, -1.f },{ 0.f, -1.5f, 0.f } }
    };

    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.5f, 0.5f, 0.5f);

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
    LambertianParams lambParams = { { 0.1f, 0.2f, 0.5f } };
    sphereMidMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    Geometry sphereRight = context->createGeometry();
    sphereRight->setPrimitiveCount(1u);
    sphereRight->setIntersectionProgram(sphereIntersect);
    sphereRight->setBoundingBoxProgram(sphereBBox);
    sphereRight["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams + 1);
    Material sphereRightMtl = context->createMaterial();
    sphereRightMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, metalMtl);
    MetalParams metalParams = { { 0.8f, 0.6f, 0.2f }, 0.f };
    sphereRightMtl["metalParams"]->setUserData(sizeof(MetalParams), &metalParams);
    GeometryInstance sphereRightGI = context->createGeometryInstance(sphereRight, &sphereRightMtl, &sphereRightMtl + 1);

    Geometry sphereLeft = context->createGeometry();
    sphereLeft->setPrimitiveCount(1u);
    sphereLeft->setIntersectionProgram(sphereIntersect);
    sphereLeft->setBoundingBoxProgram(sphereBBox);
    sphereLeft["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams + 2);
    Material sphereLeftMtl = context->createMaterial();
    sphereLeftMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, glassMtl);
    GlassParams glassParams = { { 1.f, 1.f, 1.f }, 1.5f };
    sphereLeftMtl["glassParams"]->setUserData(sizeof(glassParams), &glassParams);
    GeometryInstance sphereLeftGI = context->createGeometryInstance(sphereLeft, &sphereLeftMtl, &sphereLeftMtl + 1);

    Geometry quadFloor = context->createGeometry();
    quadFloor->setPrimitiveCount(1u);
    quadFloor->setIntersectionProgram(quadIntersect);
    quadFloor->setBoundingBoxProgram(quadBBox);
    QuadParams quadParams;
    float3 anchor = { -1000.f, -0.5f, -1000.f };
    float3 v1 = { 2000.f, 0.f, 0.f };
    float3 v2 = { 0.f, 0.f, 2000.f };
    setQuadParams(anchor, v1, v2, quadParams);
    quadFloor["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
    Material quadFloorMtl = context->createMaterial();
    quadFloorMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lambMtl);
    lambParams.albedo = make_float3(0.8f, 0.8f, 0.f);
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
    CamParams camParams;
    optix::float3 lookFrom = { 3.f, 3.f, 2.f };
    optix::float3 lookAt = { 0.f, 0.f, -1.f };
    optix::float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 20, (float)fixedWidth / (float)fixedHeight, 0.5f, length(lookFrom - lookAt), camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_COFFEE) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.f, 0.f, 0.f);
    setupScene("coffee");
    optix::float3 lookFrom = make_float3(0.f, 0.22 * aabb.extent(1), 0.25 * aabb.extent(2));
    optix::float3 lookAt = lookFrom + make_float3(0.f, -0.01875f, -1.f);
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_BEDROOM) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.f, 0.f, 0.f);
    setupScene("bedroom");
    optix::float3 lookFrom = aabb.center() + make_float3(0.3f, 0.1f, 0.45f) * aabb.extent();
    optix::float3 lookAt = aabb.center() + make_float3(0.05f, -0.1f, 0.f) * aabb.extent();
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_DININGROOM) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.f, 0.f, 0.f);
    setupScene("diningroom");
    optix::float3 lookFrom = aabb.center() + make_float3(-0.7f, 0.f, 0.f) * aabb.extent();
    optix::float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_STORMTROOPER) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.5f, 0.5f, 0.5f);
    setupScene("stormtrooper");
    optix::float3 lookFrom = aabb.center() + make_float3(0.25f, 0.1f, 0.395f) * aabb.extent();
    optix::float3 lookAt = aabb.center() + make_float3(0.25f, 0.1f, 0.f) * aabb.extent();
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 30, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_SPACESHIP) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.5f, 0.5f, 0.5f);
    setupScene("spaceship");
    optix::float3 lookFrom = aabb.center() + make_float3(-0.03f, 0.03f, -0.03f) * aabb.extent();
    optix::float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_CORNELL) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.5f, 0.5f, 0.5f);
    setupScene("cornell");
    optix::float3 lookFrom = aabb.center() + make_float3(0.f, 0.f, -2.f) * aabb.extent();
    optix::float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 39.3077, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_HYPERION || sceneId == SCENE_DRAGON) {
    Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
    context->setMissProgram(0, missProgram);
    missProgram["bgColor"]->setFloat(0.5f, 0.5f, 0.5f);
    setupScene("hyperion");
    optix::float3 lookFrom;
    if (sceneId == SCENE_HYPERION) {
      lookFrom = aabb.center() + make_float3(-0.08f, 2.f, 0.f) * aabb.extent();
    } else {
      lookFrom = aabb.center() + make_float3(0.05f, 0.3f, -0.005f) * aabb.extent();
    }
    optix::float3 lookAt = aabb.center() + make_float3(0.f, 0.f, 0.f) * aabb.extent();
    optix::float3 up = make_float3(0.f, 1.f, 0.f);
    CamParams camParams;
    setCamParams(lookFrom, lookAt, up, 30, (float)fixedWidth / (float)fixedHeight, 0.f, 1.f, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  } else if (sceneId == SCENE_SPHERES_VIDEO) {
    setUpVideo(256);
  }
}

void MinimalOptiX::setupScene(const char* sceneName) {
  nVertices = 0;
  nFaces = 0;
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
      nVertices += attrib.vertices.size() / 3;

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
      nFaces += shapes[s].mesh.num_face_vertices.size();

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
    } else if (light.shape == QUAD) {
      geo->setIntersectionProgram(quadIntersect);
      geo->setBoundingBoxProgram(quadBBox);
      QuadParams params;
      setQuadParams(light.position, light.u, light.v, params);
      geo["quadParams"]->setUserData(sizeof(QuadParams), &params);
    } else {
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

void MinimalOptiX::renderScene(bool autoSave, std::string fileNamePrefix) {
  setupScene();
  context->validate();
  uint checkpoint = 1;
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
    if (autoSave) {
      if ((i + 1) % checkpoint == 0) {
        updateContent(i + 1, false);
        saveCurrentFrame(false, fileNamePrefix + "_" + std::to_string(i + 1));
        checkpoint *= 2;
      }
    }
  }
  updateContent(nSuperSampling, true);
  if (autoSave) {
    saveCurrentFrame(false, fileNamePrefix);
  }
  qDebug() << "vertices:" << nVertices << "faces:" << nFaces;
}

void MinimalOptiX::move(SphereParams& param, float time) {
  float distance = param.velocity.y * time + time * time * videoParams.gravity / 2.0f;
  if (distance < param.center.y - param.radius + 0.5f) { // -0.5f is the plane
    param.center.x += param.velocity.x * time;
    param.center.z += param.velocity.z * time;
    param.center.y -= distance;
    param.velocity.y += videoParams.gravity * time;
  } else {
    float vend = sqrt(param.velocity.y * param.velocity.y + (2.0f * videoParams.gravity * (param.center.y - param.radius + 0.5f)));
    float t = (vend - param.velocity.y) / videoParams.gravity;
    if (t < 1e-6) {
      param.velocity.y = 0.f;
      param.center.y = -0.5f + param.radius;
      return;
    }
    param.center.x += param.velocity.x * t;
    param.center.z += param.velocity.z * t;
    param.center.y = -0.5f + param.radius;
    param.velocity.x *= videoParams.attenuationCoef;
    param.velocity.y *= videoParams.attenuationCoef;
    param.velocity.y = -vend * videoParams.attenuationCoef;
    move(param, time - t);
  }
}

void MinimalOptiX::animate(float time) {
  videoParams.angle += time * 5;
  for (size_t i = 0; i < videoParams.spheresParams.size(); ++i) {
    move(videoParams.spheresParams[i], time);
  }
}

void MinimalOptiX::record(int frames, const char* filename, bool saveFrames = false) {
  std::vector<QImage> images;
  for (int i = 0; i < frames; ++i) {
    updateVideo();
    images.push_back(canvas.copy());
    if (saveFrames) {
      std::string name = "video" + std::to_string(i);
      saveCurrentFrame(false, name);
    }
  }
  generateVideo(images, filename);
}

void MinimalOptiX::setUpVideo(int nSpheres) {
  std::vector<GeometryInstance> objs;
  Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
  context->setMissProgram(0, missProgram);
  missProgram["bgColor"]->setFloat(0.2f, 0.2f, 0.2f);
  Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
  Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
  Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
  Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
  Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
  Program metalMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "metal");
  Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
  Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");
  Program disneyMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disney");
  Program disneyAnyHit = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disneyAnyHit");
  int parameter = 4;

  std::mt19937 random(42);
  std::normal_distribution<float> stdNormal(0.f, .1f);
  std::uniform_real_distribution<float> uniform(0.f, 1.f);
  std::uniform_int_distribution<int> uniform_int(0, 2);
  bool useDisney = false;
  for (int i = 0; i < 3; ++i) {
    videoParams.spheresParams.push_back({ 3.0f,{ -10.f + 10.f * i, 2.0f, 0.f },{ 0.f, 0.f, 0.f } });
  }
  for (int i = 0; i < nSpheres; ++i) {
    float x, z, radius;
    do {
      x = uniform(random) * 30.f - 15.f;
      z = uniform(random) * 30.f - 15.f;
      radius = 1.0f;
      for (auto& param : videoParams.spheresParams) {
        radius = std::min(radius, sqrt((x - param.center.x) * (x - param.center.x) + (z - param.center.z) * (z - param.center.z)) - param.radius);
      }
      radius *= 0.8;
    } while (radius < .01f);
    float h = sqrt(x * x + z * z);
    radius = std::min(h + .5f, radius);
    videoParams.spheresParams.push_back({ radius,{ x, h, z },{ 0.f, 0.f, 0.f } });
  }
  for (int i = 0; i < 3; ++i) {
    if (useDisney) {
      DisneyParams disneyParams{ RT_TEXTURE_ID_NULL,
      { 0.2f + 0.1f * i, 0.5f + 0.1f * i, 0.5f + 0.1f * i },
     // { 0.2f + 0.1f * i, 0.2f + 0.1f * i, 0.2f + 0.1f * i },
      { 0.0f, 0.0f, 0.0f},
        0.2f + 0.3f * i, 0.2f + 0.3f * i, 0.2f + 0.3f * i, 0.3f + 0.3f * i, 0.3f + 0.3f * i,
        0.3f + 0.3f * i, 0.3f + 0.3f * i, 0.3f + 0.3f * i, 0.3f + 0.3f * i, 0.3f + 0.3f * i,
        i == 1 ? GLASS : NORMAL };
      videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &disneyParams, sphereIntersect, sphereBBox, disneyMtl, disneyAnyHit));
    } else {
      if (i == 0) {
        LambertianParams lambParams{ { 0.5f, 0.8f, 0.8f } };
        videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &lambParams, sphereIntersect, sphereBBox, lambMtl));
      } else if (i == 2) {
        float tmp = stdNormal(random) + 0.5;
        tmp = min(0.9f, max(0.1f, tmp));
        MetalParams metalParams{ { 0.9f, 0.7f, 0.7f }, tmp };
        videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &metalParams, sphereIntersect, sphereBBox, metalMtl));
      } else {
        GlassParams glassParams{ { 1.f, 1.f, 1.f }, 1.5f };
        videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &glassParams, sphereIntersect, sphereBBox, glassMtl));
      }
    }
  }
  for (int i = 3; i < videoParams.spheresParams.size(); ++i) {
    if (useDisney) {
      DisneyParams disneyParams{ RT_TEXTURE_ID_NULL,
      { uniform(random), uniform(random), uniform(random) },
      { uniform(random), uniform(random), uniform(random) },
        uniform(random), uniform(random), uniform(random), 0.5f + 0.5f * uniform(random), uniform(random),
        uniform(random), uniform(random), uniform(random), uniform(random), uniform(random),
        uniform_int(random) == 2 ? GLASS : NORMAL };
      videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &disneyParams, sphereIntersect, sphereBBox, disneyMtl, disneyAnyHit));
    } else {
      optix::float3 color{ 0.2f + 0.8f * uniform(random), 0.2f + 0.8f * uniform(random), 0.2f + 0.8f * uniform(random) };
      int type = uniform_int(random);
      if (type == 0) {
        LambertianParams lambParams{ color };
        videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &lambParams, sphereIntersect, sphereBBox, lambMtl));
      } else if (type == 1) {
        float tmp = stdNormal(random) + 0.5;
        tmp = min(0.9f, max(0.1f, tmp));
        MetalParams metalParams{ color, tmp };
        videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &metalParams, sphereIntersect, sphereBBox, metalMtl));
      } else {
        float tmp = stdNormal(random) + 2.0;
        tmp = min(3.0f, max(1.5f, tmp));
        GlassParams glassParams{ { 1.f, 1.f, 1.f }, tmp };
        videoParams.spheres.push_back(buildBall(&(videoParams.spheresParams[i]), &glassParams, sphereIntersect, sphereBBox, glassMtl));
      }
    }
  }

  Geometry quadFloor = context->createGeometry();
  quadFloor->setPrimitiveCount(1u);
  quadFloor->setIntersectionProgram(quadIntersect);
  quadFloor->setBoundingBoxProgram(quadBBox);
  QuadParams quadParams;
  float3 anchor{ -100.f, -0.5f, 100.f };
  float3 v1{ 0.f, 0.f, -200.f };
  float3 v2{ 200.f, 0.f, 0.f };
  setQuadParams(anchor, v1, v2, quadParams);
  quadFloor["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
  Material quadFloorMtl = context->createMaterial();
  quadFloorMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lambMtl);
  LambertianParams lambParams{ { 0.7f, 0.9f, 0.9f }};
  quadFloorMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
  objs.push_back(context->createGeometryInstance(quadFloor, &quadFloorMtl, &quadFloorMtl + 1));

  std::vector<GeometryInstance> lights;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      lights.push_back(buildLight({ -24.f + 10.f * i, 15.f, -24.f + 10.f*j }, { 0.f, 0.f, -8.f }, { 8.f, 0.f, 0.f }, quadIntersect, quadBBox, lightMtl));
    }
  }
  constexpr int nLight = 16;
  constexpr float angle = 3.1415926 * 2 / nLight;
  for (int i = 0; i < nLight; ++i) {
    lights.push_back(buildLight({ 40.f * sin(i * angle), 1.f, 40.f * cos(i * angle) }, { 0.f, 4.f, 0.f },
      { 10.f * sin(i * angle + angle) - 10.f * sin(i * angle), 0.f, 10.f * cos(i * angle + angle) - 10.f * cos(i * angle) }, quadIntersect, quadBBox, lightMtl));
  }


  Buffer lightBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER);
  lightBuffer->setElementSize(sizeof(LightParams));
  lightBuffer->setSize(lights.size());
  LightParams* lightBufData = (LightParams*)lightBuffer->map();
  for (int i = 0; i < lights.size(); ++i) {
    memcpy(lightBufData + i, &(lights[i]), sizeof(LightParams));
  }
  lightBuffer->unmap();
  context["lights"]->setBuffer(lightBuffer);

  for (auto& obj : videoParams.spheres) objs.push_back(obj);
  for (auto&& obj : lights) objs.push_back(obj);
  GeometryGroup geoGrp = context->createGeometryGroup();
  geoGrp->setChildCount(uint(objs.size()));
  for (auto i = 0; i < objs.size(); ++i) {
    geoGrp->setChild(i, objs[i]);
  }
  geoGrp->setAcceleration(context->createAcceleration("NoAccel"));
  context["topGroup"]->set(geoGrp);
  CamParams camParams;
  optix::float3 lookFrom = { 0.f, 8.0f, 20.f };
  optix::float3 lookAt = { 0.f, 0.f, 0.f };
  optix::float3 up = { 0.f, 1.f, 0.f };
  setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, .2f, 20.f, camParams);
  //setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, 1.0f, length(lookFrom - lookAt), camParams);
  Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
  rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
  context->setRayGenerationProgram(0, rayGenProgram);
}

void MinimalOptiX::updateVideo() {
  animate(0.002);
  for (size_t i = 0; i < videoParams.spheres.size(); ++i)
    videoParams.spheres[i]["sphereParams"]->setUserData(sizeof(SphereParams), &(videoParams.spheresParams[i]));
  CamParams camParams;
  float3 lookFrom = make_float3(20 * sin(videoParams.angle), min(12.0, videoParams.angle / 10 + 8.0), 20.f * cos(videoParams.angle));
  setCamParams(lookFrom, videoParams.lookAt, videoParams.up, 45, (float)fixedWidth / (float)fixedHeight, .2f, 20.f, camParams);
  Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "camera");
  rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
  context->setRayGenerationProgram(0, rayGenProgram);
  //context->validate();
  uint checkpoint = 1;
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
  }
  updateContent(nSuperSampling, true);
}

GeometryInstance MinimalOptiX::buildLight(float3 anchor, float3 v1, float3 v2, Program& quadIntersect, Program& quadBBox, Program& lightMtl) {
  Geometry quadLight = context->createGeometry();
  QuadParams quadParams;
  quadLight->setPrimitiveCount(1u);
  quadLight->setIntersectionProgram(quadIntersect);
  quadLight->setBoundingBoxProgram(quadBBox);
  setQuadParams(anchor, v1, v2, quadParams);
  quadLight["quadParams"]->setUserData(sizeof(QuadParams), &quadParams);
  Material quadLightMtl = context->createMaterial();
  quadLightMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lightMtl);
  LightParams lightParams;
  lightParams.emission = make_float3(1.f);
  quadLightMtl["lightParams"]->setUserData(sizeof(LightParams), &lightParams);
  return context->createGeometryInstance(quadLight, &quadLightMtl, &quadLightMtl + 1);
}

GeometryInstance MinimalOptiX::buildBall(SphereParams* sphereParams, LambertianParams* lambParams, Program& sphereIntersect, Program& sphereBBox, Program& lambMtl) {
  Geometry sphere = context->createGeometry();
  sphere->setPrimitiveCount(1u);
  sphere->setIntersectionProgram(sphereIntersect);
  sphere->setBoundingBoxProgram(sphereBBox);
  sphere["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams);
  Material sphereMtl = context->createMaterial();
  sphereMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, lambMtl);
  sphereMtl["lambParams"]->setUserData(sizeof(LambertianParams), lambParams);
  return context->createGeometryInstance(sphere, &sphereMtl, &sphereMtl + 1);
}

GeometryInstance MinimalOptiX::buildBall(SphereParams* sphereParams, MetalParams* metalParams, Program& sphereIntersect, Program& sphereBBox, Program& metalMtl) {
  Geometry sphere = context->createGeometry();
  sphere->setPrimitiveCount(1u);
  sphere->setIntersectionProgram(sphereIntersect);
  sphere->setBoundingBoxProgram(sphereBBox);
  sphere["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams);
  Material sphereMtl = context->createMaterial();
  sphereMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, metalMtl);
  sphereMtl["metalParams"]->setUserData(sizeof(MetalParams), metalParams);
  return context->createGeometryInstance(sphere, &sphereMtl, &sphereMtl + 1);
}

GeometryInstance MinimalOptiX::buildBall(SphereParams* sphereParams, GlassParams* glassParams, Program& sphereIntersect, Program& sphereBBox, Program& glassMtl) {
  Geometry sphere = context->createGeometry();
  sphere->setPrimitiveCount(1u);
  sphere->setIntersectionProgram(sphereIntersect);
  sphere->setBoundingBoxProgram(sphereBBox);
  sphere["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams);
  Material sphereMtl = context->createMaterial();
  sphereMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, glassMtl);
  sphereMtl["glassParams"]->setUserData(sizeof(GlassParams), glassParams);
  return context->createGeometryInstance(sphere, &sphereMtl, &sphereMtl + 1);
}

GeometryInstance MinimalOptiX::buildBall(SphereParams* sphereParams, DisneyParams* disneyParams, Program& sphereIntersect, Program& sphereBBox, Program& disneyMtl, Program& disneyAnyHit) {
  Geometry sphere = context->createGeometry();
  sphere->setPrimitiveCount(1u);
  sphere->setIntersectionProgram(sphereIntersect);
  sphere->setBoundingBoxProgram(sphereBBox);
  sphere["sphereParams"]->setUserData(sizeof(SphereParams), sphereParams);
  Material sphereMtl = context->createMaterial();
  sphereMtl->setClosestHitProgram(RAY_TYPE_RADIANCE, disneyMtl);
  sphereMtl->setAnyHitProgram(RAY_TYPE_SHADOW, disneyAnyHit);
  sphereMtl["disneyParams"]->setUserData(sizeof(DisneyParams), disneyParams);
  return context->createGeometryInstance(sphere, &sphereMtl, &sphereMtl + 1);
}