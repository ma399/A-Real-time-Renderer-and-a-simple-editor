#pragma once

#include "Mesh.h"
#include <Shader.h>
#include <Material.h>

// Forward declarations
class CoroutineResourceManager;

class Model {
    friend class ResourceManager;
public:

    explicit Model() = default;
    explicit Model(const Mesh* mesh, const Material* material);

    ~Model() = default;

    // Set 
    void set_mesh(const Mesh* mesh);
    void set_material(const Material* material);

    // Get
    const Mesh* get_mesh() const;
    const Material* get_material() const;

    // Check
    bool has_mesh() const;
    bool has_material() const;
    bool is_empty() const;

    // Reset
    void clear();

private:
    const Mesh* mesh_;
    const Material* material_;
};