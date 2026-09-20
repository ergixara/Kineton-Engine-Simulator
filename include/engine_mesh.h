#ifndef ATG_ENGINE_SIM_ENGINE_MESH_H
#define ATG_ENGINE_SIM_ENGINE_MESH_H

#include <array>
#include <limits>
#include <vector>
#include "learning_content.h"

class Engine;

// Presentation geometry only. Positions are in metres; the simulation owns motion.
class EngineMesh {
public:
    struct Vec3 {
        float x = 0, y = 0, z = 0;
        Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
        Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
        Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
        float dot(Vec3 b) const { return x*b.x + y*b.y + z*b.z; }
        Vec3 cross(Vec3 b) const { return {y*b.z-z*b.y, z*b.x-x*b.z, x*b.y-y*b.x}; }
        float length() const;
        Vec3 normalized() const;
    };
    struct Vertex {
        Vec3 position, normal;
        EnginePart part = EnginePart::None;
        int cylinder = -1;
    };
    enum Material {
        Aluminium, Steel, DarkSteel, BankBlue, BankRed, Brass, Grid,
        IntakeTeal, ExhaustOrange, CamPurple, Ceramic, Combustion, CombustionGlow,
        MaterialCount
    };
    struct Hit {
        EnginePart part = EnginePart::None;
        int cylinder = -1;
        float distance = std::numeric_limits<float>::infinity();
    };
    std::array<std::vector<Vertex>, MaterialCount> vertices;
    Vec3 minimum, maximum;

    void clear();
    void build(Engine *engine, bool cutaway, bool exploded = false);
    void setPart(EnginePart part, int cylinder = -1) { m_part = part; m_cylinder = cylinder; }
    Hit pick(Vec3 origin, Vec3 direction) const;
    // A negative cylinder selects the first represented instance of this part.
    bool anchor(EnginePart part, int cylinder, Vec3 *position) const;
    void cylinder(Vec3 a, Vec3 b, float radius, Material material, int segments = 24);
    void tube(Vec3 a, Vec3 b, float innerRadius, float outerRadius, Material material,
        float startAngle = 0, float sweep = 6.2831853f, int segments = 32);
    void beam(Vec3 a, Vec3 b, float width, float depth, Material material);
    Vec3 center() const { return (minimum + maximum) * 0.5f; }
    float radius() const { return (maximum - minimum).length() * 0.5f; }
    size_t vertexCount() const;

private:
    EnginePart m_part = EnginePart::None;
    int m_cylinder = -1;
    void triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 na, Vec3 nb, Vec3 nc, Material material);
    void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, Material material);
};

#endif
