#pragma once

#include <glm/glm.hpp>
#include <string>
#include <limits>
#include <functional>

// Forward declarations
class Scene;
class Model;
class Mesh;
class Camera;
class CoroutineResourceManager;
class Transform;

// Callback type for getting model transform by ID
using ModelTransformCallback = std::function<glm::mat4(const std::string& model_id)>;

// Result of a raycast operation
struct RaycastHit {
    bool hit = false;                    // Whether the ray hit something
    glm::vec3 point = glm::vec3(0.0f);   // Hit point in world coordinates
    glm::vec3 normal = glm::vec3(0.0f);  // Surface normal at hit point
    float distance = 0.0f;               // Distance from ray origin to hit point
    std::string model_id;                // ID of the hit model
    size_t triangle_index = 0;           // Index of the hit triangle
    
    // Barycentric coordinates for interpolation
    float u = 0.0f, v = 0.0f, w = 0.0f;
};

// Ray structure for raycasting
struct Ray {
    glm::vec3 origin;     // Ray origin point
    glm::vec3 direction;  // Ray direction (should be normalized)
    
    Ray(const glm::vec3& orig, const glm::vec3& dir) 
        : origin(orig), direction(glm::normalize(dir)) {}
};

/**
 * @brief System for performing ray-mesh intersection tests
 */
class RaycastSystem {
public:
    RaycastSystem();
    ~RaycastSystem();

    /**
     * @brief Perform raycast against all models in the scene
     * @param ray The ray to cast
     * @param scene The scene to test against
     * @param resource_manager Resource manager to access model data
     * @param transform_callback Callback to get model transform by ID
     * @param max_distance Maximum distance to test (optional)
     * @return RaycastHit result
     */
    RaycastHit raycast(const Ray& ray, 
                      const Scene& scene, 
                      CoroutineResourceManager& resource_manager,
                      const ModelTransformCallback& transform_callback,
                      float max_distance = std::numeric_limits<float>::max()) const;

    /**
     * @brief Perform raycast against a specific model
     * @param ray The ray to cast
     * @param model The model to test against
     * @param model_id ID of the model for result identification
     * @param model_matrix Model transformation matrix
     * @param max_distance Maximum distance to test
     * @return RaycastHit result
     */
    RaycastHit raycast_model(const Ray& ray,
                            const Model& model,
                            const std::string& model_id,
                            const glm::mat4& model_matrix = glm::mat4(1.0f),
                            float max_distance = std::numeric_limits<float>::max()) const;

    /**
     * @brief Convert screen coordinates to world ray
     * @param screen_x Screen X coordinate (0 to screen_width)
     * @param screen_y Screen Y coordinate (0 to screen_height)
     * @param screen_width Screen width in pixels
     * @param screen_height Screen height in pixels
     * @param camera Camera for view and projection matrices
     * @return Ray in world space
     */
    static Ray screen_to_world_ray(float screen_x, float screen_y,
                                  float screen_width, float screen_height,
                                  const Camera& camera);

    /**
     * @brief Test ray-triangle intersection using Möller-Trumbore algorithm
     * @param ray The ray to test
     * @param v0, v1, v2 Triangle vertices
     * @param hit Output hit information
     * @return True if intersection found
     */
    static bool ray_triangle_intersect(const Ray& ray,
                                      const glm::vec3& v0,
                                      const glm::vec3& v1,
                                      const glm::vec3& v2,
                                      RaycastHit& hit);

    /**
     * @brief Test ray-mesh intersection
     * @param ray The ray to test
     * @param mesh The mesh to test against
     * @param model_matrix Transformation matrix for the mesh
     * @param model_id ID for result identification
     * @param hit Output hit information
     * @return True if intersection found
     */
    bool ray_mesh_intersect(const Ray& ray,
                           const Mesh& mesh,
                           const glm::mat4& model_matrix,
                           const std::string& model_id,
                           RaycastHit& hit) const;

private:
    // Configuration
    bool use_backface_culling_ = true;
    float epsilon_ = 1e-8f;
};
