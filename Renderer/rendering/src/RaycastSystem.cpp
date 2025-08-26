#include "RaycastSystem.h"
#include "Scene.h"
#include "Model.h"
#include "Mesh.h"
#include "Camera.h"
#include "CoroutineResourceManager.h"
#include <Logger.h>
#include <limits>

RaycastSystem::RaycastSystem() {
    LOG_INFO("RaycastSystem: Initialized");
}

RaycastSystem::~RaycastSystem() = default;

RaycastHit RaycastSystem::raycast(const Ray& ray, 
                                 const Scene& scene, 
                                 CoroutineResourceManager& resource_manager,
                                 const ModelTransformCallback& transform_callback,
                                 float max_distance) const {
    RaycastHit closest_hit;
    closest_hit.distance = max_distance;

    // Get all models in the scene
    auto models = resource_manager.get_scene_models(scene);
    
    LOG_DEBUG("RaycastSystem: Testing ray against {} models", models.size());

    for (const auto& model : models) {
        if (!model) continue;

        // Find the model ID from the scene references
        std::string model_id = "unknown";
        const auto& model_refs = scene.get_model_references();
        
        // Find the correct model ID for this model instance
        // This is a simplified approach - match by model pointer or use index
        for (size_t i = 0; i < model_refs.size() && i < models.size(); ++i) {
            if (models[i] == model) {
                model_id = model_refs[i];
                break;
            }
        }
        
        // Get model transformation matrix from callback
        glm::mat4 model_matrix(1.0f);
        if (transform_callback && model_id != "unknown") {
            model_matrix = transform_callback(model_id);
        }
        
        LOG_DEBUG("RaycastSystem: Testing model '{}' with transform matrix", model_id);
        LOG_DEBUG("RaycastSystem: Ray origin: ({:.3f}, {:.3f}, {:.3f}), direction: ({:.3f}, {:.3f}, {:.3f})", 
                 ray.origin.x, ray.origin.y, ray.origin.z, ray.direction.x, ray.direction.y, ray.direction.z);

        RaycastHit hit = raycast_model(ray, *model, model_id, model_matrix, closest_hit.distance);
        
        if (hit.hit) {
            LOG_DEBUG("RaycastSystem: HIT found on model '{}' at distance {:.3f}, point: ({:.3f}, {:.3f}, {:.3f})", 
                     model_id, hit.distance, hit.point.x, hit.point.y, hit.point.z);
            if (hit.distance < closest_hit.distance) {
                closest_hit = hit;
            }
        } else {
            LOG_DEBUG("RaycastSystem: No hit on model '{}'", model_id);
        }
    }

    if (closest_hit.hit) {
        LOG_DEBUG("RaycastSystem: Hit found at distance {:.3f} on model '{}'", 
                 closest_hit.distance, closest_hit.model_id);
    } else {
        LOG_DEBUG("RaycastSystem: No hits found");
    }

    return closest_hit;
}

RaycastHit RaycastSystem::raycast_model(const Ray& ray,
                                       const Model& model,
                                       const std::string& model_id,
                                       const glm::mat4& model_matrix,
                                       float max_distance) const {
    RaycastHit hit;
    
    if (!model.has_mesh()) {
        return hit;
    }

    const Mesh& mesh = *model.get_mesh();
    
    if (ray_mesh_intersect(ray, mesh, model_matrix, model_id, hit)) {
        if (hit.distance <= max_distance) {
            return hit;
        }
    }

    // Return empty hit if no intersection or beyond max distance
    hit.hit = false;
    return hit;
}

Ray RaycastSystem::screen_to_world_ray(float screen_x, float screen_y,
                                      float screen_width, float screen_height,
                                      const Camera& camera) {
    // Convert screen coordinates to normalized device coordinates (NDC)
    // Screen coordinates: (0,0) at top-left, (width,height) at bottom-right
    // NDC: (-1,-1) at bottom-left, (1,1) at top-right
    float ndc_x = (2.0f * screen_x) / screen_width - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y) / screen_height;  // Flip Y axis

    // Create points in NDC space
    glm::vec4 near_point_ndc(ndc_x, ndc_y, -1.0f, 1.0f);  // Near plane
    glm::vec4 far_point_ndc(ndc_x, ndc_y, 1.0f, 1.0f);    // Far plane

    // Get camera matrices
    float aspect_ratio = screen_width / screen_height;
    glm::mat4 projection = camera.get_projection_matrix(aspect_ratio);
    glm::mat4 view = camera.get_view_matrix();
    
    // Calculate inverse matrices
    glm::mat4 inv_projection = glm::inverse(projection);
    glm::mat4 inv_view = glm::inverse(view);

    // Transform to view space
    glm::vec4 near_point_view = inv_projection * near_point_ndc;
    glm::vec4 far_point_view = inv_projection * far_point_ndc;
    
    // Perspective divide
    near_point_view /= near_point_view.w;
    far_point_view /= far_point_view.w;

    // Transform to world space
    glm::vec4 near_point_world = inv_view * near_point_view;
    glm::vec4 far_point_world = inv_view * far_point_view;

    // Create ray
    glm::vec3 ray_origin = glm::vec3(near_point_world);
    glm::vec3 ray_direction = glm::normalize(glm::vec3(far_point_world - near_point_world));

    return Ray(ray_origin, ray_direction);
}

bool RaycastSystem::ray_triangle_intersect(const Ray& ray,
                                          const glm::vec3& v0,
                                          const glm::vec3& v1,
                                          const glm::vec3& v2,
                                          RaycastHit& hit) {
    const float EPSILON = 1e-6f;  

    // MT ray-triangle intersection algorithm
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);
    
    // Ray is parallel to triangle
    if (a > -EPSILON && a < EPSILON) {
        return false;
    }
    
    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);
    
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    
    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);
    
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    
    // Calculate t 
    float t = f * glm::dot(edge2, q);
    
    if (t > EPSILON) {  // Ray intersection
        hit.hit = true;
        hit.distance = t;
        hit.point = ray.origin + t * ray.direction;
        hit.normal = glm::normalize(glm::cross(edge1, edge2));
        hit.u = u;
        hit.v = v;
        hit.w = 1.0f - u - v;
        return true;
    }
    
    return false;  
}

bool RaycastSystem::ray_mesh_intersect(const Ray& ray,
                                      const Mesh& mesh,
                                      const glm::mat4& model_matrix,
                                      const std::string& model_id,
                                      RaycastHit& hit) const {
    const auto& vertices = mesh.get_vertices();
    const auto& indices = mesh.get_indices();
    
    LOG_DEBUG("RaycastSystem: Testing mesh for model '{}' - vertices: {}, indices: {}", 
             model_id, vertices.size(), indices.size());
    
    if (indices.size() < 3 || indices.size() % 3 != 0) {
        LOG_DEBUG("RaycastSystem: Invalid mesh data for model '{}'", model_id);
        return false;
    }

    // Transform ray to model space for more efficient intersection testing
    glm::mat4 inv_model_matrix = glm::inverse(model_matrix);
    glm::vec3 local_ray_origin = glm::vec3(inv_model_matrix * glm::vec4(ray.origin, 1.0f));
    glm::vec3 local_ray_direction = glm::normalize(glm::vec3(inv_model_matrix * glm::vec4(ray.direction, 0.0f)));
    Ray local_ray(local_ray_origin, local_ray_direction);

    RaycastHit closest_hit;
    closest_hit.distance = std::numeric_limits<float>::max();
    bool found_hit = false;

    // Test each triangle
    size_t triangle_count = indices.size() / 3;
    LOG_DEBUG("RaycastSystem: Testing {} triangles for model '{}'", triangle_count, model_id);
    LOG_DEBUG("RaycastSystem: Local ray origin: ({:.3f}, {:.3f}, {:.3f}), direction: ({:.3f}, {:.3f}, {:.3f})", 
             local_ray_origin.x, local_ray_origin.y, local_ray_origin.z, 
             local_ray_direction.x, local_ray_direction.y, local_ray_direction.z);
    
    for (size_t i = 0; i < triangle_count; ++i) {
        size_t idx0 = indices[i * 3];
        size_t idx1 = indices[i * 3 + 1];
        size_t idx2 = indices[i * 3 + 2];

        if (idx0 >= vertices.size() || idx1 >= vertices.size() || idx2 >= vertices.size()) {
            continue; // Skip invalid indices
        }

        const glm::vec3& v0 = vertices[idx0].position;
        const glm::vec3& v1 = vertices[idx1].position;
        const glm::vec3& v2 = vertices[idx2].position;

        // Add debug info for plane triangles
        if (model_id == "simple_scene_plane_model") {
            LOG_DEBUG("RaycastSystem: Triangle {} vertices: v0({:.3f},{:.3f},{:.3f}), v1({:.3f},{:.3f},{:.3f}), v2({:.3f},{:.3f},{:.3f})", 
                     i, v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);
        }

        RaycastHit triangle_hit;
        if (ray_triangle_intersect(local_ray, v0, v1, v2, triangle_hit)) {
            if (model_id == "simple_scene_plane_model") {
                LOG_DEBUG("RaycastSystem: Triangle {} HIT at distance {:.3f}", i, triangle_hit.distance);
            }
            if (triangle_hit.distance < closest_hit.distance) {
                closest_hit = triangle_hit;
                closest_hit.triangle_index = i;
                found_hit = true;
            }
        } else if (model_id == "simple_scene_plane_model") {
            LOG_DEBUG("RaycastSystem: Triangle {} NO HIT", i);
        }
    }

    if (found_hit) {
        // Transform hit point and normal back to world space
        closest_hit.point = glm::vec3(model_matrix * glm::vec4(closest_hit.point, 1.0f));
        closest_hit.normal = glm::normalize(glm::vec3(glm::transpose(inv_model_matrix) * glm::vec4(closest_hit.normal, 0.0f)));
        closest_hit.model_id = model_id;
        
        // Recalculate distance in world space
        closest_hit.distance = glm::length(closest_hit.point - ray.origin);
        
        hit = closest_hit;
        return true;
    }

    return false;
}
