#include "MinimalOptiX.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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
  setupScene(SCENE_MESH_TEST);
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
  missProgram["bgColor"]->setFloat(1.f, 1.f, 1.f);
}

void MinimalOptiX::setupScene(SceneId sceneId) {
  if (sceneId == SCENE_BASIC_TEST) {
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
    SphereParams sphereParams = { 0.5f, {0.f, 0.f, -1.f} };
    sphereMid["sphereParams"]->setUserData(sizeof(SphereParams), &sphereParams);
    optix::Material sphereMidMtl = context->createMaterial();
    sphereMidMtl->setClosestHitProgram(0, lambMtl);
    LambertianParams lambParams = { {0.1f, 0.2f, 0.5f}, defaultNScatter, 1 };
    sphereMidMtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);
    optix::GeometryInstance sphereMidGI = context->createGeometryInstance(sphereMid, &sphereMidMtl, &sphereMidMtl + 1);

    optix::Geometry sphereRight = context->createGeometry();
    sphereRight->setPrimitiveCount(1u);
    sphereRight->setIntersectionProgram(sphereIntersect);
    sphereRight->setBoundingBoxProgram(sphereBBox);
    sphereParams.center = optix::make_float3(1.f, 0.f, -1.f);
    sphereRight["sphereParams"]->setUserData(sizeof(SphereParams), &sphereParams);
    optix::Material sphereRightMtl = context->createMaterial();
    sphereRightMtl->setClosestHitProgram(0, metalMtl);
    MetalParams metalParams = { {0.8f, 0.6f, 0.2f}, 0.f };
    sphereRightMtl["metalParams"]->setUserData(sizeof(MetalParams), &metalParams);
    optix::GeometryInstance sphereRightGI = context->createGeometryInstance(sphereRight, &sphereRightMtl, &sphereRightMtl + 1);

    optix::Geometry sphereLeft = context->createGeometry();
    sphereLeft->setPrimitiveCount(1u);
    sphereLeft->setIntersectionProgram(sphereIntersect);
    sphereLeft->setBoundingBoxProgram(sphereBBox);
    sphereParams.center = optix::make_float3(-1.f, 0.f, -1.f);
    sphereLeft["sphereParams"]->setUserData(sizeof(SphereParams), &sphereParams);
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
    context["topGroup"]->set(geoGrp);

    // camera
    CamParams camParams;
    optix::float3 lookFrom = { 0.f, 1.f, 2.f };
    optix::float3 lookAt = { 0.f, 0.f, -1.f };
    optix::float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    optix::Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
    // ======================================================================================================================
  } else if (sceneId == SCENE_MESH_TEST) { // ===============================================================================
    // ======================================================================================================================
    // objects
    optix::Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
    optix::Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
    optix::Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
    optix::Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
    optix::Program meshIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshIntersect");
    optix::Program meshBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshBBox");
    optix::Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
    optix::Program metalMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "metal");
    optix::Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
    optix::Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");
    optix::Program disneyMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disney");

    optix::Geometry geo = context->createGeometry();
    geo->setPrimitiveCount(4u);
    geo->setIntersectionProgram(meshIntersect);
    geo->setBoundingBoxProgram(meshBBox);

    optix::Buffer vertexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, 4u);
    float vertBufSrc[] = {
      0.f, 0.f, -0.75f,
      0.5f, 0.f, 0.25f,
      -0.5f, 0.f, 0.25f,
      0.f, 1.f, 0.f
    };
    memcpy(vertexBuffer->map(), vertBufSrc, sizeof(float) * 12);
    vertexBuffer->unmap();
    geo["vertexBuffer"]->set(vertexBuffer);

    optix::Buffer normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, 0);
    geo["normalBuffer"]->set(normalBuffer);
    optix::Buffer texcoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, 0);
    geo["texcoordBuffer"]->set(texcoordBuffer);


    optix::Buffer indexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, 4u);
    int idxBufSrc[] = {
      1, 0, 3,
      1, 3, 2,
      3, 0, 2,
      0, 1, 2
    };
    memcpy(indexBuffer->map(), idxBufSrc, sizeof(int) * 12);
    indexBuffer->unmap();
    geo["indexBuffer"]->set(indexBuffer);

    optix::Material mtl = context->createMaterial();
    mtl->setClosestHitProgram(0, lambMtl);
    LambertianParams lambParams = { { 0.3f, 0.5f, 0.7f }, 16, 1 };
    mtl["lambParams"]->setUserData(sizeof(LambertianParams), &lambParams);

    optix::GeometryInstance gi = context->createGeometryInstance(geo, &mtl, &mtl + 1);

    optix::GeometryGroup grp = context->createGeometryGroup();
    grp->setChildCount(1u);
    grp->setChild(0, gi);
    grp->setAcceleration(context->createAcceleration("NoAccel"));
    context["topGroup"]->set(grp);

    // Camera
    CamParams camParams;
    auto lookFrom = optix::make_float3(0.f, 0.3f, -2.f);
    auto lookAt = optix::make_float3(0.f, 0.3f, 0.f);
    auto up = optix::make_float3(0.f, 1.f, 0.f);
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    optix::Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
    // ======================================================================================================================
  } else if (sceneId == SCENE_COFFEE) { // ==================================================================================
    // ======================================================================================================================
    // objects
    optix::Program sphereIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereIntersect");
    optix::Program sphereBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "sphereBBox");
    optix::Program quadIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadIntersect");
    optix::Program quadBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "quadBBox");
    optix::Program meshIntersect = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshIntersect");
    optix::Program meshBBox = context->createProgramFromPTXString(ptxStrs[geoCuFileName], "meshBBox");
    optix::Program lambMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "lambertian");
    optix::Program metalMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "metal");
    optix::Program lightMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "light");
    optix::Program glassMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "glass");
    optix::Program disneyMtl = context->createProgramFromPTXString(ptxStrs[mtlCuFileName], "disney");

    std::string sceneFolder = baseSceneFolder + "coffee/";
    Scene scene((sceneFolder + "coffee.scene").c_str());

    optix::Aabb aabb;

    optix::GeometryGroup meshGroup = context->createGeometryGroup();
    meshGroup->setChildCount(uint(scene.meshNames.size()));
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
        optix::Geometry geo = context->createGeometry();
        geo->setPrimitiveCount(uint(attrib.vertices.size() / 3));
        geo->setIntersectionProgram(meshIntersect);
        geo->setBoundingBoxProgram(meshBBox);

        optix::Buffer vertexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, attrib.vertices.size());
        memcpy(vertexBuffer->map(), &attrib.vertices[0], sizeof(float) * attrib.vertices.size());
        vertexBuffer->unmap();
        geo["vertexBuffer"]->set(vertexBuffer);

        if (!attrib.normals.empty()) {
          optix::Buffer normalBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, attrib.normals.size());
          memcpy(normalBuffer->map(), &attrib.normals[0], sizeof(float) * attrib.normals.size());
          normalBuffer->unmap();
          geo["normalBuffer"]->set(normalBuffer);
        }

        if (!attrib.texcoords.empty()) {
          optix::Buffer texcoordBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, attrib.texcoords.size());
          memcpy(texcoordBuffer->map(), &attrib.texcoords[0], sizeof(float) * attrib.texcoords.size());
          texcoordBuffer->unmap();
          geo["texcoordBuffer"]->set(texcoordBuffer);
        }

        optix::Buffer indexBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, shapes[s].mesh.indices.size());
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
            aabb.include(optix::make_float3(vx, vy, vz));
          }
        }
        indexBuffer->unmap();
        geo["indexBuffer"]->set(indexBuffer);

        // load material
        optix::Material mtl = context->createMaterial();
        mtl->setClosestHitProgram(0, disneyMtl);
        mtl["disneyParams"]->setUserData(sizeof(DisneyParams), &scene.materials[i]);

        optix::GeometryInstance meshGI = context->createGeometryInstance(geo, &mtl, &mtl + 1);
        meshGroup->addChild(meshGI);
      }
    }

    // lights
    optix::GeometryGroup lightGroup = context->createGeometryGroup();
    lightGroup->setChildCount(uint(scene.lights.size()));
    lightGroup->setAcceleration(context->createAcceleration("Trbvh"));
    for (auto& light : scene.lights) {
      optix::Geometry geo = context->createGeometry();
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

      optix::Material mtl = context->createMaterial();
      mtl->setClosestHitProgram(0, lightMtl);
      mtl["lightParams"]->setUserData(sizeof(LightParams), &light);

      optix::GeometryInstance gi = context->createGeometryInstance(geo, &mtl, &mtl + 1);
      lightGroup->addChild(gi);
    }

    optix::Group topGroup = context->createGroup();
    topGroup->setChildCount(2);
    topGroup->setAcceleration(context->createAcceleration("Trbvh"));
    topGroup->addChild(meshGroup);
    topGroup->addChild(lightGroup);
    context["topGroup"]->set(topGroup);

    // camera
    CamParams camParams;
    optix::float3 lookFrom = 1.5f * aabb.extent();
    optix::float3 lookAt = aabb.center();
    optix::float3 up = { 0.f, 1.f, 0.f };
    setCamParams(lookFrom, lookAt, up, 45, (float)fixedWidth / (float)fixedHeight, camParams);
    optix::Program rayGenProgram = context->createProgramFromPTXString(ptxStrs[camCuFileName], "pinholeCamera");
    rayGenProgram["camParams"]->setUserData(sizeof(CamParams), &camParams);
    context->setRayGenerationProgram(0, rayGenProgram);
  }

}
