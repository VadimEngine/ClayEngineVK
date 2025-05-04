// standard lib
#include <iostream>
// third party
// glm
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// clay
#include "clay/application/App.h"
// class
#include "clay/application/Scene.h"


const std::string MODEL_PATH = std::string(RESOURCE_PATH) + "/models/viking_room.obj";
const std::string TEXTURE_PATH = std::string(RESOURCE_PATH) + "/models/viking_room.png";

Scene::Scene(App& app) : mApp_(app) {}

Scene::~Scene() {}

void Scene::initialize() {
    std::unique_ptr<Model> model1 = std::make_unique<Model>();

    FileData modelFile = loadFileToMemory(MODEL_PATH);
    model1->parseModelFile(modelFile);

    mModels_.emplace_back(std::move(model1));

    mCamera_.setPosition({0,0,3});
}

void Scene::update(float dt) {
    const InputHandler& inputHandler = *(mApp_.getWindow().getInputHandler());

   if (inputHandler.isKeyPressed(GLFW_KEY_W)) {
       mCamera_.move(mCamera_.getForward(), mCamera_.getMoveSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_S)) {
       mCamera_.move(mCamera_.getForward(), -mCamera_.getMoveSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_A)) {
       mCamera_.move(mCamera_.getRight(), -mCamera_.getMoveSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_D)) {
       mCamera_.move(mCamera_.getRight(), mCamera_.getMoveSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_SPACE)) {
       mCamera_.move(mCamera_.getUp(), mCamera_.getMoveSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
       mCamera_.move(mCamera_.getUp(), -mCamera_.getMoveSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_LEFT)) {
       mCamera_.rotate(mCamera_.getUp(), mCamera_.getRotationSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_RIGHT)) {
       mCamera_.rotate(mCamera_.getUp(), -mCamera_.getRotationSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_UP)) {
       mCamera_.rotate(mCamera_.getRight(), mCamera_.getRotationSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_DOWN)) {
       mCamera_.rotate(mCamera_.getRight(), -mCamera_.getRotationSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_Q)) {
       mCamera_.rotate(mCamera_.getForward(), 200.0f * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_E)) {
       mCamera_.rotate(mCamera_.getForward(), -200.0f * dt);
   }
   // Speed
   if (inputHandler.isKeyPressed(GLFW_KEY_PERIOD)) {
       mCamera_.zoom(-mCamera_.getZoomSpeed() * dt);
   }
   if (inputHandler.isKeyPressed(GLFW_KEY_COMMA)) {
       mCamera_.zoom(mCamera_.getZoomSpeed() * dt);
   }
}

void Scene::render() {

}