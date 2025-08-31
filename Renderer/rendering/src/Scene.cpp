#include "Scene.h"
#include <algorithm>

void Scene::add_renderable_reference(const std::string& renderable_id) {
    if (std::find(renderable_references_.begin(), renderable_references_.end(), renderable_id) == renderable_references_.end()) {
        renderable_references_.push_back(renderable_id);
    }
}

void Scene::remove_renderable_reference(const std::string& renderable_id) {
    auto it = std::find(renderable_references_.begin(), renderable_references_.end(), renderable_id);
    if (it != renderable_references_.end()) {
        renderable_references_.erase(it);
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

