#include "Renderable.h"
#include <algorithm>

Renderable::Renderable(const std::string& id) : id_(id) {}

void Renderable::add_model(const std::string& model_id) {
    auto it = std::find(model_ids_.begin(), model_ids_.end(), model_id);
    if (it == model_ids_.end()) {
        model_ids_.push_back(model_id);
    }
}

void Renderable::remove_model(const std::string& model_id) {
    auto it = std::find(model_ids_.begin(), model_ids_.end(), model_id);
    if (it != model_ids_.end()) {
        model_ids_.erase(it);
    }
}

const std::vector<std::string>& Renderable::get_model_ids() const {
    return model_ids_;
}

bool Renderable::has_models() const {
    return !model_ids_.empty();
}

void Renderable::set_visible(bool visible) {
    visible_ = visible;
}

bool Renderable::is_visible() const {
    return visible_;
}

void Renderable::set_material_override(const std::string& material_id) {
    material_override_ = material_id;
}

void Renderable::clear_material_override() {
    material_override_.clear();
}

const std::string& Renderable::get_material_override() const {
    return material_override_;
}

bool Renderable::has_material_override() const {
    return !material_override_.empty();
}

const std::string& Renderable::get_id() const {
    return id_;
}
