#include "Scene.h"
#include <algorithm>

void Scene::add_model_reference(const std::string& modelId) {
    if (std::find(model_references_.begin(), model_references_.end(), modelId) == model_references_.end()) {
        model_references_.push_back(modelId);
    }
}

void Scene::remove_model_reference(const std::string& modelId) {
    auto it = std::find(model_references_.begin(), model_references_.end(), modelId);
    if (it != model_references_.end()) {
        model_references_.erase(it);
    }
}

void Scene::add_light_reference(const std::string& lightId) {
    if (std::find(light_references_.begin(), light_references_.end(), lightId) == light_references_.end()) {
        light_references_.push_back(lightId);
    }
}

void Scene::remove_light_reference(const std::string& lightId) {
    auto it = std::find(light_references_.begin(), light_references_.end(), lightId);
    if (it != light_references_.end()) {
        light_references_.erase(it);
    }
}

