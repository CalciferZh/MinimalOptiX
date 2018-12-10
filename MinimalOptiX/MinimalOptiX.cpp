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
  setupScene(SCENE_BASIC_TEST);
  context->validate();
  for (uint i = 0; i < nSuperSampling; ++i) {
    context["randSeed"]->setInt(randSeed());
    context->launch(0, fixedWidth, fixedHeight);
  }
  update(ACCU_BUFFER);
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
      }
    }
    context["accuBuffer"]->getBuffer()->unmap();
  }

  QPixmap tmpPixmap = QPixmap::fromImage(canvas);
  qgscene.clear();
  qgscene.addPixmap(tmpPixmap);
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
  context = Context::create();
  context->setRayTypeCount(1);
  context->setEntryPointCount(1);
  context->setStackSize(9608);

  context["rayTypeRadiance"]->setUint(0);
  context["rayMaxDepth"]->setUint(rayMaxDepth);
  context["rayMinIntensity"]->setFloat(rayMinIntensity);
  context["rayEpsilonT"]->setFloat(rayEpsilonT);
  context["absorbColor"]->setFloat(0.f, 0.f, 0.f);
  context["nSuperSampling"]->setUint(nSuperSampling);


  Buffer outputBuffer = context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_UNSIGNED_BYTE4, fixedWidth, fixedHeight);
  context["outputBuffer"]->set(outputBuffer);
  Buffer accuBuffer = context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT3, fixedWidth, fixedHeight);
  context["accuBuffer"]->set(accuBuffer);

  // Exception
  Program exptProgram = context->createProgramFromPTXString(ptxStrs[exCuFileName], "exception");
  context->setExceptionProgram(0, exptProgram);
  context["badColor"]->setFloat(1.f, 0.f, 0.f);

  // Miss
  Program missProgram = context->createProgramFromPTXString(ptxStrs[msCuFileName], "staticMiss");
  context->setMissProgram(0, missProgram);
  //missProgram["bgColor"]->setFloat(1.f, 1.f, 1.f);
  missProgram["bgColor"]->setFloat(0.3f, 0.3f, 0.3f);
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
    SphereParams sphereParams = { 0.5f, {0.f, 0.f, -1.f} };
    sphereMid["sphereParams"]->setUserData(sizeof(SphereParams), &sphereParams);
    Material sphereMidMtl = context->createMaterial();
    sphereMidMtl->setClosestHitProgram(0, lambMtl);
    LambertianParams lambParams = { {0.1f, 0.2f, 0.5f}, defaultNScatter, 1 };
    sphereMidMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    Geometry sphereRight = context->createGeometry();
    sphereRight->setPrimitiveCount(1u);
    sphereRight->setIntersectionProgram(sphereIntersect);
    sphereRight->setBoundingBoxProgram(sphereBBox);
    sphereParams.center = make_float3(1.f, 0.f, -1.f);
    sphereRight["sphereParams"]->setUserData(sizeof(SphereParams), &sphereParams);
    Material sphereRightMtl = context->createMaterial();
    sphereRightMtl->setClosestHitProgram(0, metalMtl);
    MetalParams metalParams = { {0.8f, 0.6f, 0.2f}, 0.f };
    sphereRightMtl["metalParams"]->setUserData(sizeof(MetalParams), &metalParams);
    GeometryInstance sphereRightGI = context->createGeometryInstance(sphereRight, &sphereRightMtl, &sphereRightMtl + 1);

    Geometry sphereLeft = context->createGeometry();
    sphereLeft->setPrimitiveCount(1u);
    sphereLeft->setIntersectionProgram(sphereIntersect);
    sphereLeft->setBoundingBoxProgram(sphereBBox);
    sphereParams.center = make_float3(-1.f, 0.f, -1.f);
    sphereLeft["sphereParams"]->setUserData(sizeof(SphereParams), &sphereParams);
    Material sphereLeftMtl = context->createMaterial();
    sphereLeftMtl->setClosestHitProgram(0, glassMtl);
    GlassParams glassParams = { {1.f, 1.f, 1.f}, 1.5f, defaultNScatter, 1 };
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
    quadFloorMtl->setClosestHitProgram(0, lambMtl);
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
    quadLightMtl->setClosestHitProgram(0, lightMtl);
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

    // camera
    CamParams camParams;
    float3 lookFrom = { 0.f, 1.f, 2.f };
    float3 lookAt = { 0.f, 0.f, -1.f };
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
    // ======================================================================================================================
  } else if (sceneId == SCENE_MESH_TEST) { // ===============================================================================
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
    mtl->setClosestHitProgram(0, lambMtl);
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
    // ======================================================================================================================
  } else if (sceneId == SCENE_COFFEE) { // ==================================================================================
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

    std::string sceneFolder = baseSceneFolder + "coffee/";
    Scene scene((sceneFolder + "coffee.scene").c_str());

    Aabb aabb;

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
        // load geometry
        Geometry geo = context->createGeometry();
        geo->setPrimitiveCount(shapes[s].mesh.num_face_vertices.size());
        geo->setIntersectionProgram(meshIntersect);
        geo->setBoundingBoxProgram(meshBBox);

        Buffer vertexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, attrib.vertices.size());
        memcpy(vertexBuffer->map(), attrib.vertices.data(), sizeof(float) * attrib.vertices.size());
        vertexBuffer->unmap();
        geo["vertexBuffer"]->set(vertexBuffer);

        Buffer normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, attrib.normals.size());
        if (!attrib.normals.empty()) {
          memcpy(normalBuffer->map(), attrib.normals.data(), sizeof(float) * attrib.normals.size());
          normalBuffer->unmap();
        }
        geo["normalBuffer"]->set(normalBuffer);

        Buffer texcoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, attrib.texcoords.size());
        if (!attrib.texcoords.empty()) {
          memcpy(texcoordBuffer->map(), attrib.texcoords.data(), sizeof(float) * attrib.texcoords.size());
          texcoordBuffer->unmap();
        }
        geo["texcoordBuffer"]->set(texcoordBuffer);

        Buffer indexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, shapes[s].mesh.num_face_vertices.size());
        int* indexBufDst = (int*)indexBuffer->map();
        for (int f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
          if (shapes[s].mesh.num_face_vertices[f] != 3) {
            throw std::logic_error("Face's number of vertices != 3");
          }
          for (int fv = 0; fv < 3; ++fv) {
            auto idx = shapes[s].mesh.indices[f * 3 + fv];
            indexBufDst[f * 3 + fv] = idx.vertex_index;
            tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
            tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
            tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
            aabb.include(make_float3(vx, vy, vz));
          }
        }
        indexBuffer->unmap();
        geo["indexBuffer"]->set(indexBuffer);

        // load material
        Material mtl = context->createMaterial();
        mtl->setClosestHitProgram(0, disneyMtl);
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
      mtl->setClosestHitProgram(0, lightMtl);
      mtl["lightParams"]->setUserData(sizeof(LightParams), &light);

      GeometryInstance gi = context->createGeometryInstance(geo, &mtl, &mtl + 1);
      lightGroup->addChild(gi);
    }

    Group topGroup = context->createGroup();
    topGroup->setAcceleration(context->createAcceleration("Trbvh"));
    topGroup->addChild(meshGroup);
    topGroup->addChild(lightGroup);
    context["topGroup"]->set(topGroup);

    // camera
    CamParams camParams;
    float3 lookFrom = 1.5f * aabb.extent();
    float3 lookAt = aabb.center();
    float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }

}
