#include "DragHandler.h"
#include "RaycastSystem.h"
#include "ObjectTransformSystem.h"
#include "Camera.h"
#include "Scene.h"
#include "CoroutineResourceManager.h"
#include "Transform.h"
#include <Logger.h>

DragHandler::DragHandler()
    : scene_(nullptr),
      resource_manager_(nullptr),
      enabled_(true),
      is_dragging_(false) {
}

DragHandler::~DragHandler() {
    cleanup();
}

bool DragHandler::initialize(std::shared_ptr<Camera> camera,
                           Scene* scene,
                           CoroutineResourceManager* resource_manager) {
    if (!camera || !scene || !resource_manager) {
        LOG_ERROR("DragHandler: Invalid parameters for initialization");
        return false;
    }

    camera_ = camera;
    scene_ = scene;
    resource_manager_ = resource_manager;

    // Create raycast system
    raycast_system_ = std::make_shared<RaycastSystem>();
    
    // Create transform system
    transform_system_ = std::make_shared<ObjectTransformSystem>();
    if (!transform_system_->initialize(raycast_system_)) {
        LOG_ERROR("DragHandler: Failed to initialize transform system");
        return false;
    }

    LOG_INFO("DragHandler: Initialized successfully");
    return true;
}

void DragHandler::cleanup() {
    if (is_dragging_) {
        end_drag();
    }
    
    transform_system_.reset();
    raycast_system_.reset();
    camera_.reset();
    scene_ = nullptr;
    resource_manager_ = nullptr;
}

void DragHandler::set_drag_start_callback(DragStartCallback callback) {
    drag_start_callback_ = callback;
}

void DragHandler::set_drag_update_callback(DragUpdateCallback callback) {
    drag_update_callback_ = callback;
}

void DragHandler::set_drag_end_callback(DragEndCallback callback) {
    drag_end_callback_ = callback;
}

bool DragHandler::start_drag(float screen_x, float screen_y, 
                           float viewport_width, float viewport_height) {
    if (!enabled_ || !transform_system_ || !camera_ || !scene_ || !resource_manager_) {
        return false;
    }

    if (is_dragging_) {
        LOG_WARN("DragHandler: Already dragging, ending previous drag");
        end_drag();
    }

    bool drag_started = transform_system_->start_drag(
        screen_x, screen_y,
        viewport_width, viewport_height,
        *camera_, *scene_, *resource_manager_
    );
    
    if (drag_started) {
        is_dragging_ = true;
        const auto& drag_info = transform_system_->get_drag_info();
        
        LOG_INFO("DragHandler: Started dragging model '{}' at ({}, {})", 
                drag_info.model_id, screen_x, screen_y);
        
        if (drag_start_callback_) {
            drag_start_callback_(drag_info.model_id, screen_x, screen_y);
        }
        
        return true;
    } else {
        LOG_DEBUG("DragHandler: No object found at screen position ({}, {})", screen_x, screen_y);
        return false;
    }
}

bool DragHandler::update_drag(float screen_x, float screen_y,
                            float viewport_width, float viewport_height) {
    if (!is_dragging_ || !transform_system_ || !camera_) {
        return false;
    }

    bool drag_updated = transform_system_->update_drag(
        screen_x, screen_y,
        viewport_width, viewport_height,
        *camera_
    );
    
    if (drag_updated) {
        const auto& drag_info = transform_system_->get_drag_info();
        
        LOG_DEBUG("DragHandler: Updated drag for model '{}' at ({}, {})", 
                 drag_info.model_id, screen_x, screen_y);
        
        if (drag_update_callback_) {
            drag_update_callback_(drag_info.model_id, screen_x, screen_y);
        }
        
        return true;
    }
    
    return false;
}

void DragHandler::end_drag() {
    if (!is_dragging_ || !transform_system_) {
        return;
    }

    const auto& drag_info = transform_system_->get_drag_info();
    std::string model_id = drag_info.model_id;
    
    transform_system_->end_drag();
    is_dragging_ = false;
    
    LOG_INFO("DragHandler: Ended dragging model '{}'", model_id);
    
    if (drag_end_callback_) {
        drag_end_callback_(model_id);
    }
}

bool DragHandler::is_dragging() const {
    return is_dragging_ && transform_system_ && transform_system_->is_dragging();
}

Transform DragHandler::get_model_transform(const std::string& model_id) const {
    if (transform_system_) {
        return transform_system_->get_transform(model_id);
    }
    return Transform::identity();
}

ObjectTransformSystem* DragHandler::get_transform_system() const {
    return transform_system_.get();
}