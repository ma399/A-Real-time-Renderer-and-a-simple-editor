#include "RaycastUtils.h"
#include "Scene.h"
#include "Model.h"
#include "Mesh.h"
#include "Camera.h"
#include "CoroutineResourceManager.h"
#include <Logger.h>
#include <limits>

Ray RaycastUtils::screen_to_world_ray(float screen_x, float screen_y,
                                     float screen_width, float screen_height,
                                     const Camera& camera) {
    // Convert screen coordinates to normalized device coordinates (NDC)
    float ndc_x = (2.0f * screen_x) / screen_width - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y) / screen_height;
    
    // Create NDC point
    glm::vec4 ndc_point(ndc_x, ndc_y, -1.0f, 1.0f);
    
    // Get camera matrices
    glm::mat4 view_matrix = camera.get_view_matrix();
    float aspect_ratio = screen_width / screen_height;
    glm::mat4 projection_matrix = camera.get_projection_matrix(aspect_ratio);
    
    // Calculate inverse matrices
    glm::mat4 inv_projection = glm::inverse(projection_matrix);
    glm::mat4 inv_view = glm::inverse(view_matrix);
    
    // Transform to view space
    glm::vec4 view_point = inv_projection * ndc_point;
    view_point.z = -1.0f;
    view_point.w = 0.0f;
    
    // Transform to world space
    glm::vec4 world_direction = inv_view * view_point;
    
    // Create ray
    glm::vec3 ray_origin = camera.get_position();
    glm::vec3 ray_direction = glm::normalize(glm::vec3(world_direction));
    
    return Ray(ray_origin, ray_direction);
}

RaycastHit RaycastUtils::raycast_scene(const Ray& ray, 
                                      const Scene& scene, 
                                      CoroutineResourceManager& resource_manager,
                                      std::function<glm::mat4(const std::string&)> get_transform_callback,
                                      float max_distance) {
    RaycastHit closest_hit;
    closest_hit.distance = max_distance;

    // Get all renderables in the scene
    auto renderables = resource_manager.get_scene_renderables(scene);
    const auto& renderable_refs = scene.get_renderable_references();
    
    LOG_DEBUG("RaycastUtils: Testing ray against {} renderables", renderables.size());

    for (size_t i = 0; i < renderables.size() && i < renderable_refs.size(); ++i) {
        const auto& renderable = renderables[i];
        if (!renderable || !renderable->is_visible() || !renderable->has_models()) continue;

        const std::string& renderable_id = renderable_refs[i];
        
        // Get the actual transform matrix for this renderable
        glm::mat4 renderable_matrix = get_transform_callback ? get_transform_callback(renderable_id) : glm::mat4(1.0f);
        
        // Test each model in the renderable
        for (const auto& model_id : renderable->get_model_ids()) {
            auto model = resource_manager.get<Model>(model_id);
            if (!model || !model->has_mesh()) continue;
            
            RaycastHit hit;
            if (ray_mesh_intersect(ray, *model->get_mesh(), renderable_matrix, renderable_id, hit)) {
                if (hit.distance < closest_hit.distance) {
                    closest_hit = hit;
                }
            }
        }
    }

    return closest_hit;
}

bool RaycastUtils::ray_triangle_intersect(const Ray& ray,
                                         const glm::vec3& v0,
                                         const glm::vec3& v1,
                                         const glm::vec3& v2,
                                         RaycastHit& hit) {
    // MT ray-triangle intersection algorithm
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);
    
    if (a > -EPSILON && a < EPSILON) {
        return false; // Ray is parallel to triangle
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
    
    float t = f * glm::dot(edge2, q);
    
    if (t > EPSILON) {
        hit.hit = true;
        hit.distance = t;
        hit.point = ray.origin + ray.direction * t;
        hit.normal = glm::normalize(glm::cross(edge1, edge2));
        hit.u = u;
        hit.v = v;
        hit.w = 1.0f - u - v;
        return true;
    }
    
    return false;
}

bool RaycastUtils::ray_mesh_intersect(const Ray& ray,
                                     const Mesh& mesh,
                                     const glm::mat4& model_matrix,
                                     const std::string& model_id,
                                     RaycastHit& hit) {
    const auto& vertices = mesh.get_vertices();
    const auto& indices = mesh.get_indices();
    
    if (indices.size() < 3 || indices.size() % 3 != 0) {
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
    for (size_t i = 0; i < triangle_count; ++i) {
        size_t idx0 = indices[i * 3];
        size_t idx1 = indices[i * 3 + 1];
        size_t idx2 = indices[i * 3 + 2];
        
        if (idx0 >= vertices.size() || idx1 >= vertices.size() || idx2 >= vertices.size()) {
            continue;
        }
        
        glm::vec3 v0 = vertices[idx0].position;
        glm::vec3 v1 = vertices[idx1].position;
        glm::vec3 v2 = vertices[idx2].position;
        
        RaycastHit triangle_hit;
        if (ray_triangle_intersect(local_ray, v0, v1, v2, triangle_hit)) {
            if (triangle_hit.distance < closest_hit.distance) {
                closest_hit = triangle_hit;
                closest_hit.triangle_index = i;
                found_hit = true;
            }
        }
    }
    
    if (found_hit) {
        // Transform hit point back to world space
        closest_hit.point = glm::vec3(model_matrix * glm::vec4(closest_hit.point, 1.0f));
        closest_hit.normal = glm::normalize(glm::vec3(glm::transpose(inv_model_matrix) * glm::vec4(closest_hit.normal, 0.0f)));
        closest_hit.model_id = model_id;
        hit = closest_hit;
        return true;
    }
    
    return false;
}
