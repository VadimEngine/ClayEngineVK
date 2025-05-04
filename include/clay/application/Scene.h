#pragma once
// standard lib
#include <vector>
#include <memory>
// clay
#include "clay/utils/Utils.h"
#include "clay/graphics/Model.h"
#include "clay/graphics/Camera.h"

class App;

class Scene {
public:

    Scene(App& app);

    ~Scene();

    void initialize();

    void update(float dt);

    void render();

    std::vector<Vertex>  mAssimpVertices_;
    std::vector<uint32_t> mAssimpIndices_;

    std::vector<std::unique_ptr<Model>> mModels_;

    Camera mCamera_;

    App& mApp_;

};