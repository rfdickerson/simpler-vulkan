#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// Simple 2D Simplex Noise implementation
// Based on Ken Perlin's improved noise and Stefan Gustavson's simplex noise
class SimplexNoise {
public:
    explicit SimplexNoise(uint32_t seed = 0) {
        // Initialize permutation table with seed
        std::vector<int> p(256);
        for (int i = 0; i < 256; ++i) {
            p[i] = i;
        }
        
        // Shuffle using seed
        uint32_t rng = seed;
        for (int i = 255; i > 0; --i) {
            rng = (rng * 1103515245 + 12345) & 0x7fffffff; // LCG
            int j = rng % (i + 1);
            std::swap(p[i], p[j]);
        }
        
        // Duplicate permutation table
        perm.resize(512);
        for (int i = 0; i < 512; ++i) {
            perm[i] = p[i & 255];
        }
    }
    
    // 2D Simplex noise - returns value in range [-1, 1]
    float noise(float x, float y) const {
        // Skewing factors for 2D simplex grid
        const float F2 = 0.5f * (std::sqrt(3.0f) - 1.0f);
        const float G2 = (3.0f - std::sqrt(3.0f)) / 6.0f;
        
        // Skew input space to determine which simplex cell we're in
        float s = (x + y) * F2;
        int i = fastFloor(x + s);
        int j = fastFloor(y + s);
        
        float t = (i + j) * G2;
        float X0 = i - t;
        float Y0 = j - t;
        float x0 = x - X0;
        float y0 = y - Y0;
        
        // Determine which simplex we're in
        int i1, j1;
        if (x0 > y0) {
            i1 = 1; j1 = 0;
        } else {
            i1 = 0; j1 = 1;
        }
        
        // Offsets for middle and last corners
        float x1 = x0 - i1 + G2;
        float y1 = y0 - j1 + G2;
        float x2 = x0 - 1.0f + 2.0f * G2;
        float y2 = y0 - 1.0f + 2.0f * G2;
        
        // Work out hashed gradient indices
        int ii = i & 255;
        int jj = j & 255;
        int gi0 = perm[ii + perm[jj]] % 12;
        int gi1 = perm[ii + i1 + perm[jj + j1]] % 12;
        int gi2 = perm[ii + 1 + perm[jj + 1]] % 12;
        
        // Calculate contribution from three corners
        float n0, n1, n2;
        float t0 = 0.5f - x0*x0 - y0*y0;
        if (t0 < 0.0f) {
            n0 = 0.0f;
        } else {
            t0 *= t0;
            n0 = t0 * t0 * dot(grad3[gi0], x0, y0);
        }
        
        float t1 = 0.5f - x1*x1 - y1*y1;
        if (t1 < 0.0f) {
            n1 = 0.0f;
        } else {
            t1 *= t1;
            n1 = t1 * t1 * dot(grad3[gi1], x1, y1);
        }
        
        float t2 = 0.5f - x2*x2 - y2*y2;
        if (t2 < 0.0f) {
            n2 = 0.0f;
        } else {
            t2 *= t2;
            n2 = t2 * t2 * dot(grad3[gi2], x2, y2);
        }
        
        // Add contributions and scale to [-1, 1]
        return 70.0f * (n0 + n1 + n2);
    }
    
    // Fractal/Octave noise for more natural-looking terrain
    // Returns value in range [0, 1]
    float fractalNoise(float x, float y, int octaves, float persistence, float lacunarity = 2.0f) const {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float maxValue = 0.0f;  // Used for normalizing
        
        for (int i = 0; i < octaves; ++i) {
            total += noise(x * frequency, y * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }
        
        // Normalize to [0, 1]
        return (total / maxValue + 1.0f) * 0.5f;
    }
    
private:
    std::vector<int> perm;
    
    // Gradient vectors for 2D (12 directions)
    static constexpr float grad3[12][3] = {
        {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
        {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
        {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1}
    };
    
    static int fastFloor(float x) {
        int xi = static_cast<int>(x);
        return x < xi ? xi - 1 : xi;
    }
    
    static float dot(const float g[3], float x, float y) {
        return g[0] * x + g[1] * y;
    }
};

// Initialize static gradient table
constexpr float SimplexNoise::grad3[12][3];

// Helper function for generating 2D elevation maps
inline std::vector<std::vector<float>> generateElevationMap(int width, int height, 
                                                             uint32_t seed,
                                                             float frequency = 0.08f,
                                                             int octaves = 4,
                                                             float persistence = 0.5f) {
    SimplexNoise noise(seed);
    std::vector<std::vector<float>> elevationMap(height, std::vector<float>(width));
    
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            float x = col * frequency;
            float y = row * frequency;
            elevationMap[row][col] = noise.fractalNoise(x, y, octaves, persistence);
        }
    }
    
    return elevationMap;
}


