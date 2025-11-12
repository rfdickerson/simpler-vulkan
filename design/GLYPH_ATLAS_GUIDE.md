# Glyph Atlas System for Vulkan Text Rendering

## Overview

This system provides VkImage creation with VMA and a glyph atlas for text rendering in Vulkan.

## Files Created

### 1. `src/image.hpp` & `src/image.cpp`
Provides VkImage creation and management using VMA (Vulkan Memory Allocator):

- **`Image` struct**: Contains VkImage, VmaAllocation, VkImageView, format, and dimensions
- **`createImage()`**: Creates a VkImage with VMA allocation
- **`createImageView()`**: Creates a VkImageView for the image
- **`destroyImage()`**: Cleans up image resources
- **`transitionImageLayout()`**: Uses Vulkan 1.3 synchronization2 for layout transitions
- **`copyBufferToImage()`**: Copies buffer data to an image
- **`uploadImageData()`**: High-level function to upload data to an image (creates staging buffer internally)

### 2. `src/glyph_atlas.hpp` & `src/glyph_atlas.cpp`
Manages a texture atlas for font glyphs:

- **`GlyphAtlas` class**: Main atlas manager
  - Packs glyph bitmaps into a single texture
  - Stores UV coordinates and metrics for each glyph
  - Integrates with FreeType for rasterization
  
- **`GlyphInfo` struct**: Contains:
  - UV coordinates in the atlas
  - Glyph dimensions and bearing
  - Horizontal advance

- **`createAtlasSampler()`**: Creates a VkSampler for the atlas texture

### 3. `src/text_renderer.hpp`
Defines the `TextRenderer` helper that ties together shaping, atlas management, vertex uploads, and draw commands.

## Quick Start

### Basic Usage

```cpp
#include "glyph_atlas.hpp"
#include "text.hpp" // HbShaper

// 1. Create atlas
GlyphAtlas atlas(device, 2048, 2048);

// 2. Load font
atlas.loadFont("assets/fonts/EBGaramond-Regular.ttf", 48);

// 3. Shape text with HarfBuzz
HbShaper shaper("assets/fonts/EBGaramond-Regular.ttf", 48);
auto shapedGlyphs = shaper.shape_utf8("Hello, World!");

// 4. Add glyphs to atlas
for (const auto& g : shapedGlyphs) {
    atlas.addGlyph(g.glyph_index);
}

// 5. Finalize atlas (uploads to GPU)
VkCommandBuffer cmd = ...; // Your command buffer
atlas.finalizeAtlas(cmd);

// 6. Create sampler
VkSampler sampler = createAtlasSampler(device);

// 7. Use in rendering
const Image& atlasImage = atlas.getAtlasImage();
// Bind atlasImage.view and sampler to your descriptor set
```

### Rendering Loop

```cpp
// For each glyph in shaped text
for (const auto& sg : shapedGlyphs) {
    const GlyphInfo* info = atlas.getGlyphInfo(sg.glyph_index);
    if (info) {
        // Create quad with:
        // - Position: cursorX + sg.x_offset + info->bearingX, cursorY + sg.y_offset - info->bearingY
        // - Size: info->width, info->height
        // - UV: info->uvX, uvY, uvW, uvH
        
        // Advance cursor
        cursorX += sg.x_advance;
        cursorY += sg.y_advance;
    }
}
```

## Shader Example

```glsl
// Fragment shader
layout(binding = 0) uniform sampler2D glyphAtlas;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

uniform vec4 textColor = vec4(1.0, 1.0, 1.0, 1.0);

void main() {
    float alpha = texture(glyphAtlas, fragUV).r;
    outColor = vec4(textColor.rgb, textColor.a * alpha);
}
```

## Key Features

### VkImage with VMA
- Automatic memory allocation and management
- Support for staging buffer uploads
- Layout transitions using Vulkan 1.3's synchronization2
- Easy-to-use API

### Glyph Atlas
- Single-channel (R8) texture format for efficiency
- Simple left-to-right, top-to-bottom packing algorithm
- Configurable atlas size (default 2048x2048)
- Padding between glyphs to prevent bleeding
- FreeType integration for glyph rasterization

### Integration with HarfBuzz
- Works seamlessly with your existing `HbShaper` class
- Supports complex text shaping (ligatures, kerning, etc.)
- Uses glyph indices from HarfBuzz output

## Architecture Notes

1. **Image Format**: Uses `VK_FORMAT_R8_UNORM` (single-channel 8-bit) for the atlas, which is efficient for font rendering

2. **Memory Usage**: Images use `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE` for optimal GPU access

3. **Synchronization**: Uses Vulkan 1.3's `VkImageMemoryBarrier2` and `vkCmdPipelineBarrier2` for modern synchronization

4. **Atlas Finalization**: The atlas must be finalized before use. After finalization, no more glyphs can be added

5. **Glyph Packing**: Simple greedy algorithm; for production, consider more sophisticated bin-packing algorithms

## CMake Integration

The new files have been added to `CMakeLists.txt`:

```cmake
add_executable (CMakeProject7 
    "src/main.cpp" 
    "src/vma_impl.cpp" 
    "src/device.cpp" 
    "src/buffer.cpp" 
    "src/window.cpp" 
    "src/image.cpp"         # New
    "src/glyph_atlas.cpp"   # New
)
```

## Next Steps

1. Create a command buffer for atlas finalization
2. Set up descriptor sets for the atlas texture and sampler
3. Create vertex/index buffers for text quads
4. Write vertex and fragment shaders
5. Implement text rendering pipeline

## Example Rendering Flow

The snippet below illustrates how to prepare and draw screen text with the new `TextRenderer` abstraction:

```cpp
TextRenderer textRenderer(device, swapchain, "assets/fonts/EBGaramond-Regular.ttf", 48);

// Shape and cache glyphs before finalizing the atlas
textRenderer.setText("Hello, World!", 32.0f, 64.0f);

// Upload atlas & vertex data (staging buffer destroyed after submission)
Buffer staging = textRenderer.finalizeAtlas(uploadCmd);
textRenderer.uploadVertexData();

// Later, during rendering
textRenderer.recordDraw(cmd, swapchain.extent, glm::vec4(1.0f));
```

