#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <limits>
#include <functional>

// Forward declarations
class Scene;
class Model;
class Mesh;
class Camera;
class CoroutineResourceManager;

// Result of a raycast operation
struct RaycastHit {
    bool hit = false;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
    std::string model_id;
    size_t triangle_index = 0;
    float u = 0.0f, v = 0.0f, w = 0.0f;
};

// Ray structure for raycasting
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    
    Ray(const glm::vec3& orig, const glm::vec3& dir) 
        : origin(orig), direction(glm::normalize(dir)) {}
};

class RaycastUtils {
public:
    
    static Ray screen_to_world_ray(float screen_x, float screen_y,
                                  float screen_width, float screen_height,
                                  const Camera& camera);

    static RaycastHit raycast_scene(const Ray& ray, 
                                   const Scene& scene, 
                                   CoroutineResourceManager& resource_manager,
                                   std::function<glm::mat4(const std::string&)> get_transform_callback,
                                   float max_distance = std::numeric_limits<float>::max());

    static bool ray_triangle_intersect(const Ray& ray,
                                      const glm::vec3& v0,
                                      const glm::vec3& v1,
                                      const glm::vec3& v2,
                                      RaycastHit& hit);

    static bool ray_mesh_intersect(const Ray& ray,
                                  const Mesh& mesh,
                                  const glm::mat4& model_matrix,
                                  const std::string& model_id,
                                  RaycastHit& hit);

private:
    static constexpr float EPSILON = 1e-8f;
};
