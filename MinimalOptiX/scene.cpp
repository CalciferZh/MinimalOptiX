#include "scene.h"

static const int kMaxLineLength = 2048;

Scene::Scene(const char* fileName) {
  int texId = 0;
  FILE* file = fopen(fileName, "r");

  if (!file) {
    printf("Couldn't open %s for reading.", fileName);
  }

  std::map<std::string, DisneyParams> materialMap;
  std::map<std::string, int> textureId;

  char line[kMaxLineLength];

  while (fgets(line, kMaxLineLength, file)) {
    // skip comments
    if (line[0] == '#') {
      continue;
    }

    // name used for materials and meshes
    char name[kMaxLineLength] = { 0 };

    // Material
    if (sscanf(line, " material %s", name) == 1) {
      printf("%s", line);

      DisneyParams material;
      char texName[kMaxLineLength] = "None";

      while (fgets(line, kMaxLineLength, file)) {
        if (strchr(line, '}')) {
          break;
        }
        sscanf(line, " name %s", name);
        sscanf(line, " color %f %f %f", &material.color.x, &material.color.y, &material.color.z);
        sscanf(line, " albedoTex %s", &texName);
        sscanf(line, " emission %f %f %f", &material.emission.x, &material.emission.y, &material.emission.z);
        sscanf(line, " metallic %f", &material.metallic);
        sscanf(line, " subsurface %f", &material.subsurface);
        sscanf(line, " specular %f", &material.specular);
        sscanf(line, " specularTint %f", &material.specularTint);
        sscanf(line, " roughness %f", &material.roughness);
        sscanf(line, " anisotropic %f", &material.anisotropic);
        sscanf(line, " sheen %f", &material.sheen);
        sscanf(line, " sheenTint %f", &material.sheenTint);
        sscanf(line, " clearcoat %f", &material.clearcoat);
        sscanf(line, " clearcoatGloss %f", &material.clearcoatGloss);
        sscanf(line, " brdf %i", &material.brdf);
      }

      // Check if texture is already seen
      if (textureId.find(texName) != textureId.end()) {
        material.albedoID = textureId[texName];
      } else if (strcmp(texName, "None") != 0) {
        texId++;
        textureId[texName] = texId;
        textureMap[texId - 1] = texName;
        material.albedoID = texId;
      }

      // add material to map
      materialMap[name] = material;
    }

    // Light
    if (strstr(line, "light")) {
      LightParams light;
      optix::float3 v1;
      optix::float3 v2;
      char lightType[20] = "None";

      while (fgets(line, kMaxLineLength, file)) {
        if (strchr(line, '}')) {
          break;
        }
        sscanf(line, " position %f %f %f", &light.position.x, &light.position.y, &light.position.z);
        sscanf(line, " emission %f %f %f", &light.emission.x, &light.emission.y, &light.emission.z);
        sscanf(line, " normal %f %f %f", &light.normal.x, &light.normal.y, &light.normal.z);
        sscanf(line, " radius %f", &light.radius);
        sscanf(line, " v1 %f %f %f", &v1.x, &v1.y, &v1.z);
        sscanf(line, " v2 %f %f %f", &v2.x, &v2.y, &v2.z);
        sscanf(line, " type %s", lightType);
      }

      if (strcmp(lightType, "Quad") == 0) {
        light.shape = QUAD;
        light.u = v1 - light.position;
        light.v = v2 - light.position;
        light.area = optix::length(optix::cross(light.u, light.v));
        light.normal = optix::normalize(optix::cross(light.u, light.v));
      } else if (strcmp(lightType, "Sphere") == 0) {
        light.shape = SPHERE;
        light.normal = optix::normalize(light.normal);
        light.area = 4.0f * M_PIf * light.radius * light.radius;
      }
      lights.push_back(light);
    }

    // Property
    if (strstr(line, "properties")) {
      while (fgets(line, kMaxLineLength, file)) {
        if (strchr(line, '}')) {
          break;
        }
        sscanf(line, " width %i", &width);
        sscanf(line, " height %i", &height);
      }
    }

    // Mesh
    if (strstr(line, "mesh")) {
      while (fgets(line, kMaxLineLength, file)) {
        if (strchr(line, '}'))
          break;
        int count = 0;
        char name[2048];
        if (sscanf(line, " file %s", name) == 1) {
          meshNames.push_back(name);
        }
        if (sscanf(line, " material %s", name) == 1) {
          if (materialMap.find(name) != materialMap.end()) {
            materials.push_back(materialMap[name]);
          } else {
            printf("Could not find material %s\n", name);
          }
        }
      }
    }
  }
}
