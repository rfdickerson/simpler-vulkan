#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <glm/glm.hpp>

// Hex coordinate system using axial coordinates (q, r)
// Reference: https://www.redblobgames.com/grids/hexagons/
struct HexCoord {
    int q; // column
    int r; // row
    
    HexCoord() : q(0), r(0) {}
    HexCoord(int q_, int r_) : q(q_), r(r_) {}
    
    // Cube coordinate conversion (for some operations)
    int s() const { return -q - r; }
    
    bool operator==(const HexCoord& other) const {
        return q == other.q && r == other.r;
    }
    
    bool operator!=(const HexCoord& other) const {
        return !(*this == other);
    }
    
    HexCoord operator+(const HexCoord& other) const {
        return HexCoord(q + other.q, r + other.r);
    }
    
    HexCoord operator-(const HexCoord& other) const {
        return HexCoord(q - other.q, r - other.r);
    }
    
    HexCoord operator*(int scale) const {
        return HexCoord(q * scale, r * scale);
    }
};

// Hex direction vectors (flat-top orientation)
inline const std::array<HexCoord, 6> HEX_DIRECTIONS = {{
    HexCoord(1, 0),   // East
    HexCoord(1, -1),  // Northeast
    HexCoord(0, -1),  // Northwest
    HexCoord(-1, 0),  // West
    HexCoord(-1, 1),  // Southwest
    HexCoord(0, 1)    // Southeast
}};

// Get neighbor in a specific direction
inline HexCoord hexNeighbor(const HexCoord& hex, int direction) {
    return hex + HEX_DIRECTIONS[direction];
}

// Get all 6 neighbors
inline std::vector<HexCoord> hexNeighbors(const HexCoord& hex) {
    std::vector<HexCoord> neighbors;
    neighbors.reserve(6);
    for (int i = 0; i < 6; ++i) {
        neighbors.push_back(hexNeighbor(hex, i));
    }
    return neighbors;
}

// Distance between two hexes (in hex steps)
inline int hexDistance(const HexCoord& a, const HexCoord& b) {
    return (std::abs(a.q - b.q) + std::abs(a.r - b.r) + std::abs(a.s() - b.s())) / 2;
}

// Convert hex coordinates to world position (flat-top orientation)
inline glm::vec3 hexToWorld(const HexCoord& hex, float hexSize) {
    float x = hexSize * (3.0f/2.0f * hex.q);
    float z = hexSize * (std::sqrt(3.0f)/2.0f * hex.q + std::sqrt(3.0f) * hex.r);
    return glm::vec3(x, 0.0f, -z);  // Negate Z to fix upside-down view
}

// Convert world position to hex coordinates (returns fractional, needs rounding)
inline HexCoord worldToHex(const glm::vec3& worldPos, float hexSize) {
    float q = (2.0f/3.0f * worldPos.x) / hexSize;
    float r = (-1.0f/3.0f * worldPos.x - std::sqrt(3.0f)/3.0f * worldPos.z) / hexSize;  // Negate z term to match negated Z in hexToWorld
    
    // Cube rounding
    float s = -q - r;
    int rq = static_cast<int>(std::round(q));
    int rr = static_cast<int>(std::round(r));
    int rs = static_cast<int>(std::round(s));
    
    float q_diff = std::abs(rq - q);
    float r_diff = std::abs(rr - r);
    float s_diff = std::abs(rs - s);
    
    if (q_diff > r_diff && q_diff > s_diff) {
        rq = -rr - rs;
    } else if (r_diff > s_diff) {
        rr = -rq - rs;
    }
    
    return HexCoord(rq, rr);
}

// Get hex vertices in world space (flat-top orientation)
inline std::array<glm::vec3, 6> hexVertices(const HexCoord& hex, float hexSize, float height = 0.0f) {
    glm::vec3 center = hexToWorld(hex, hexSize);
    center.y = height;
    
    std::array<glm::vec3, 6> vertices;
    for (int i = 0; i < 6; ++i) {
        float angle_deg = 60.0f * i;  // Start at 0° for flat-top
        float angle_rad = glm::radians(angle_deg);
        vertices[i] = center + glm::vec3(
            hexSize * std::cos(angle_rad),
            0.0f,
            hexSize * std::sin(angle_rad)
        );
        vertices[i].y = height;
    }
    return vertices;
}

// Get all hexes in a radius around a center hex
inline std::vector<HexCoord> hexesInRadius(const HexCoord& center, int radius) {
    std::vector<HexCoord> results;
    for (int q = -radius; q <= radius; ++q) {
        int r1 = std::max(-radius, -q - radius);
        int r2 = std::min(radius, -q + radius);
        for (int r = r1; r <= r2; ++r) {
            results.push_back(center + HexCoord(q, r));
        }
    }
    return results;
}

// Hash function for HexCoord (for use in unordered_map)
namespace std {
    template<>
    struct hash<HexCoord> {
        size_t operator()(const HexCoord& hex) const {
            // Cantor pairing function
            size_t h1 = std::hash<int>{}(hex.q);
            size_t h2 = std::hash<int>{}(hex.r);
            return h1 ^ (h2 << 1);
        }
    };
}

