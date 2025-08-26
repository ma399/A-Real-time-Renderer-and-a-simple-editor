#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 WorldPos;
out vec3 Tangent;
out vec4 PrevFragPos;  // For motion vectors

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 prevModelViewProjection;  // Previous frame's MVP matrix

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    FragPos = WorldPos;
    
    // Transform normal and tangent to world space
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * aNormal);
    Tangent = normalize(normalMatrix * aTangent);
    
    TexCoords = aTexCoords;
    
    // Calculate current position
    gl_Position = projection * view * vec4(WorldPos, 1.0);
    
    // Calculate previous frame position for motion vectors
    PrevFragPos = prevModelViewProjection * vec4(aPos, 1.0);
}
