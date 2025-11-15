# Terrain PBR Field Blending

This renderer now shades the board-game style hex terrain by blending continuous biome fields instead of assigning a single fixed material per hex. The shader derives slope, moisture, and temperature fields, converts them into biome weights, and then blends physically-based material parameters from a small set of authored material archetypes.

## Data written to the terrain uniform buffer

| Field | Description |
| --- | --- |
| `sunDirSnow.xyz` | Normalized sun direction. |
| `sunDirSnow.w` | Snow line expressed in normalized elevation (0–1). |
| `sunColorAmbient.xyz` | Sun light color. |
| `sunColorAmbient.w` | Ambient light intensity. |
| `fieldScales.x` | Hex size (world units). |
| `fieldScales.y` | Moisture field tiling scale. |
| `fieldScales.z` | Temperature field tiling scale. |
| `fieldScales.w` | Shared biome material tiling scale. |
| `fieldBias.x` | Temperature lapse rate vs. elevation. |
| `fieldBias.y` | Global moisture bias. |
| `fieldBias.z` | Slope-to-rock weighting strength. |
| `fieldBias.w` | Additional rock/sand bias along river tiles. |
| `elevationInfo.x` | Minimum terrain elevation (world units). |
| `elevationInfo.y` | Maximum terrain elevation (world units). |
| `elevationInfo.z` | Shared detail noise scale for materials. |
| `eraInfo.x` | Stylization era selector (Discovery/Enlightenment/Industrial). |

The CPU populates these values via `TerrainParamsUBO`, so designers can tune the blend without touching shader code.

## Shader overview

1. **Field derivation** – The shader computes:
   - slope from the geometric normal,
   - elevation normalized between the min/max values supplied in the UBO,
   - procedural moisture and temperature using FBM noise seeded per-hex,
   - tile masks (rivers, wetlands, deserts, forests) from the authoring data.

2. **Biome weights** – Moisture, temperature, slope, and masks feed fuzzy-membership curves that return weights for grass, rock, sand, and snow archetypes. Slope and river biases in the uniform adjust the mix; snow is clamped to higher elevations.

3. **Material sampling** – Each archetype uses a lightweight procedural "atlas" created with FBM noise to vary albedo, roughness, and a pseudo normal perturbation. All archetypes share tiling data from the uniform so they stay visually coherent.

4. **PBR lighting** – The shader blends the archetype parameters, perturbs the normal, and evaluates a lambert + gloss lighting model with specular power derived from the blended roughness. SSAO is sampled and applied late in the shading chain.

5. **Board-game clarity** – Hex silhouettes still get the subtle inked edge treatment, and UI-era grading happens as a final post color correction, so the overall scene keeps the readable tabletop presentation.

Water tiles continue to use the bespoke reflection shader, but moisture/temperature still modulate their tint so they harmonize with nearby land.

## Updating shaders

Run your existing shader compilation script (e.g. `glslc terrain.frag -o terrain.frag.spv`) after editing the GLSL to refresh the SPIR-V binaries consumed at runtime.
