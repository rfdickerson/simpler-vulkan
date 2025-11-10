# 🌍 Art & Terrain Design Document — “Age of Exploration” Visual Style

**Project Codename:** Clockwork Empires  
**Engine:** Vulkan (1.4+)  
**Art Direction:** Stylized “Age of Exploration” board-game diorama inspired by *Civ V*, *Civ VI*, *Golden Compass*, and *Songs of Conquest.*

---

## 🎨 1. Artistic Vision Overview

The world should evoke the **Age of Discovery → Enlightenment → Early Industrial** aesthetic arc, starting with a *painterly map come to life.*

**Keywords:**  
Parchment, brass, atlases, resin seas, hand-painted hills, and diorama charm.

**Tone:**  
Curiosity, optimism, craftsmanship — humanity pushing into the unknown with compass, sextant, and courage.

**Visual Philosophy:**
- Every tile looks **crafted by hand** — beveled like a board-game piece.
- The world should feel **tactile** (wood grain, embossed paper, brass gleam).
- Lighting is **golden and soft**, as though the world sits on a desk under morning sunlight.
- The camera resembles a **tilt-shift macro lens** looking down at miniatures.

---

## 🧩 2. Hex Tile Rendering (Vulkan)

**Goal:** Efficiently render stylized terrain using procedural shading and minimal mesh complexity.

### Hex Tile System
- The terrain is a **single subdivided plane mesh**.
- A **hex index texture** defines boundaries and terrain type per tile.
- Use **tessellation shaders** for smooth curvature and subtle displacement.
- Hex borders are drawn via **edge masks** or screen-space outlines.

### Shading Pipeline
1. **Base Triplanar Blend:** Blend between three terrain textures (e.g., sand, grass, rock) using height masks.
2. **Terrain AO / Curvature:** Multiply baked AO to enhance miniature depth.
3. **Edge Highlight:** Subtle rim lighting or Fresnel to emulate hand-painted edge light.
4. **Animated Layer (optional):** 
   - Water ripples (scrolling normal maps)
   - Fog movement
   - Heat shimmer or ambient wind distortion
5. **Era LUT Color Grade:** Apply LUTs per era (Discovery → Enlightenment → Industrial).
6. **Miniature Camera FX:**
   - Tilt-shift DOF
   - Subtle vignette and bloom
   - Tonemapped highlights for resin-like light play

### Material Style
| Material | Description | Shader Notes |
|-----------|--------------|---------------|
| **Water** | Semi-transparent, refractive, resin-like | Animated normal map, depth-based color shift, shoreline foam |
| **Land** | Painted clay/wood hybrid | Triplanar blend, curvature highlights |
| **Mountains** | Sculpted, snow-capped | Steeper displacement; snow mask from normal.y |
| **Forest Canopy** | Stylized card clusters | Depth fog + ambient sway animation |
| **City / Structures** | Brass, wood, stone | Custom PBR materials with stylized AO |

---

## 🌄 3. Terrain Types (Age of Exploration Focus)

| Terrain | Visual Description | Shader/Effect | Gameplay Theme |
|----------|--------------------|----------------|----------------|
| **Ocean / Sea** | Deep teal watercolor, cross-hatched like maps | Animated normals, foam near coasts | Navigation, trade, exploration |
| **Coast / Shallow Sea** | Bright turquoise, visible reefs | Refraction and shoreline blending | Early settlements, fish resources |
| **Grassland / Plains** | Sunlit ochre and golden greens | Slight wind shader, ambient AO | Fertile core land |
| **Forest / Jungle** | Painted canopy with shadowed depths | Dappled light, canopy sway | Wood, spices, mystery |
| **Hills** | Rounded shapes, layered color | Height-based blend + rim light | Tactical height |
| **Mountains** | Embossed, clay-sculpted ridges | Tessellation displacement + snow mask | Natural barrier |
| **Desert / Dunes** | Parchment-toned sand, contour lines | Animated heat haze, high roughness | Harsh travel, rare resources |
| **Swamp / Marsh** | Muddy reflections, mist | Normal map ripples, fog layer | Slow terrain |
| **Tundra / Ice** | Blue-white permafrost | Subsurface scattering, fog overlay | Frontier terrain |
| **Rivers** | Ink-like flow, dynamic | Flow map animation, depth color | Trade and growth |
| **Natural Wonders** | Unique volumetric FX | Custom material and emissive | Discovery rewards |

---

## 🧭 4. Fog of Discovery

Unexplored areas appear as **parchment map** with decorative compass roses and faded ink text (“Here Be Dragons”).

Shader behavior:
- Desaturate terrain → replace with parchment albedo.
- Animated reveal brush (circle mask expanding from explorers).
- Faint edge glow as color fades in.

---

## 🏙️ 5. Lighting & Mood

**Primary Light:** Low-angle sunlight (golden hour tone)  
**Ambient:** Warm bounce (simulate global illumination via LUT or cubemap)  
**Night:** Blue fog ambient, city emissives glow softly (lanterns, torches)  

**Era progression** modifies global LUT:
- **Discovery:** warm, saturated, optimistic.
- **Enlightenment:** neutral whites and cool marble tones.
- **Victorian:** desaturated, foggy, industrial.

---

## 🛠️ 6. Blender Asset Scope (Minimalist)

Only model silhouettes that require unique outlines or moving parts.

| Category | Asset | Notes |
|-----------|--------|-------|
| **Mountains** | 2–3 base meshes | Reuse and scale; baked normals |
| **Trees** | 2–3 stylized clusters | Billboards or low-poly cards |
| **Buildings** | Cottage, lighthouse, windmill, harbor | Distinct silhouettes per era |
| **Ships** | Caravel, galleon | Animated sails |
| **Props** | Compass, obelisk, ruins | For landmarks or scene dressing |

Everything else should be achieved with shaders and procedural texture blending.

---

## 🧮 7. Technical Recommendations (Vulkan)

- **Dynamic Rendering** with bindless descriptors for terrain/material atlases.  
- **Compute-based culling** for hex tiles outside frustum.  
- **Timeline semaphores** for terrain-LOD transitions.  
- **Deferred or hybrid PBR lighting**, tuned for stylized falloff.  
- **VMA (Vulkan Memory Allocator)** for dynamic buffer reuse.  
- **LUT swaps** for era transitions without recompiling pipelines.

---

## 🗺️ 8. References & Inspiration

| Source | Reference Aspect |
|---------|------------------|
| *Sid Meier’s Civilization V & VI* | Readability and color coding |
| *Songs of Conquest* | Miniature diorama tone |
| *His Dark Materials (Golden Compass)* | Brass instrumentation and mystical realism |
| *Dishonored / Arcanum* | Late-industrial atmosphere |
| *Settlers of Catan* | Board-game tactility |
| Historical Atlases (16th–18th c.) | Terrain shading style |

---

## ✨ 9. Guiding Principle

> “If it moves or blends, shader it. If it casts a silhouette, model it.”  
> — Art Direction Mantra

The world should feel like a **living map**—half-painted, half-alive—always on the brink of discovery.

---

**Document Version:** 1.0  
**Author:** Rob (Solo Developer)  
**Last Updated:** November 2025
