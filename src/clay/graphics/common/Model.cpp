// class
#include "clay/graphics/common/Model.h"

namespace clay {

// TODO load from file logic

// move constructor
Model::Model(Model&& other) noexcept {
    mModelGroups_ = std::move(other.mModelGroups_);
}

// move assignment
Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        mModelGroups_ = std::move(other.mModelGroups_);
    }
    return *this;
}

Model::~Model() {}

void Model::addElement(const ModelElement& element) {
    mModelGroups_.push_back(element);
}

} // namespace clay