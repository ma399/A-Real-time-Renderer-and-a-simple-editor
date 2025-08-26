#include "Model.h"
#include "Logger.h"

Model::Model(const Mesh* mesh, const Material* material) 
    : mesh_(mesh), material_(material) { 
    LOG_INFO("Model: Creating model with mesh and material observers");
}

void Model::set_mesh(const Mesh* mesh) {
    LOG_DEBUG("Model: Setting mesh observer");
    mesh_ = mesh;
}

void Model::set_material(const Material* material) {
    LOG_DEBUG("Model: Setting material observer");
    material_ = material;
}

const Mesh* Model::get_mesh() const {
    return mesh_;
}

const Material* Model::get_material() const {
    return material_;
}

bool Model::has_mesh() const {
    return mesh_ != nullptr;
}

bool Model::has_material() const {
    return material_ != nullptr;
}

bool Model::is_empty() const {
    return mesh_ == nullptr;
}

void Model::clear() {
    if (mesh_ || material_) {
        LOG_INFO("Model: Clearing mesh and material observers");
        mesh_ = nullptr;
        material_ = nullptr;
    }
}