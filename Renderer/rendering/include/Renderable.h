#pragma once

#include <string>
#include <vector>

class Renderable {
public:
    explicit Renderable(const std::string& id);
    ~Renderable() = default;

    void add_model(const std::string& model_id);
    void remove_model(const std::string& model_id);
    const std::vector<std::string>& get_model_ids() const;
    bool has_models() const;

    void set_visible(bool visible);
    bool is_visible() const;

    void set_material_override(const std::string& material_id);
    void clear_material_override();
    const std::string& get_material_override() const;
    bool has_material_override() const;

    const std::string& get_id() const;

private:
    std::string id_;
    std::vector<std::string> model_ids_;
    bool visible_ = true;
    std::string material_override_;
};
