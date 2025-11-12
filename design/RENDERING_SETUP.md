# Text Rendering System - Complete Setup

This document describes the complete swapchain and pipeline setup for Vulkan text rendering.

## Overview

You now have a fully functional text rendering system with:
- Swapchain management with automatic recreation on resize
- Dynamic rendering (Vulkan 1.3)
- Alpha-blended text rendering
- Glyph atlas texture system
- Complete frame synchronization

## Files Created/Modified

### Swapchain Management
- **`src/swapchain.hpp`** & **`src/swapchain.cpp`**
  - Creates and manages Vulkan swapchain
  - Handles window surface creation via GLFW
  - Automatic swapchain recreation on resize
  - Frame synchronization (double buffering with semaphores and fences)
  - Image acquisition and presentation

### Text Rendering Pipeline
- **`src/text_pipeline.hpp`** & **`src/text_pipeline.cpp`**
  - Graphics pipeline for text rendering
  - Descriptor sets for glyph atlas texture
  - Command buffer management
  - Push constants for screen size and text color
  - Alpha blending for text

### Shaders
- **`shaders/text.vert`** - Vertex shader
  - Converts pixel coordinates to NDC
  - Push constants for screen size
  
- **`shaders/text.frag`** - Fragment shader
  - Samples glyph atlas texture
  - Alpha blending with configurable text color
  - Push constants for color

- **`shaders/compile_shaders.bat`** - Compilation script
- **`shaders/text.vert.spv`** & **`shaders/text.frag.spv`** - Compiled SPIR-V

### Modified Files
- **`src/device.cpp`**
  - Added GLFW extension support for surface creation
  - Added VK_KHR_SWAPCHAIN_EXTENSION_NAME device extension

- **`src/main.cpp`**
  - Complete rendering loop
  - Vertex buffer creation and management
  - Text rendering with shaped glyphs
  - Frame synchronization

- **`CMakeLists.txt`**
  - Added swapchain.cpp and text_pipeline.cpp

## Architecture

### Frame Flow

```
1. Window Init
   ↓
2. Vulkan Init (Instance + Device)
   ↓
3. Surface Creation (GLFW → Vulkan)
   ↓
4. Swapchain Creation
   ↓
5. Glyph Atlas Setup
   ↓
6. Pipeline Creation
   ↓
7. Render Loop:
   - Acquire Next Image
   - Record Commands
     * Transition to COLOR_ATTACHMENT
     * Begin Dynamic Rendering
     * Bind Pipeline & Descriptors
     * Draw Text
     * End Rendering
     * Transition to PRESENT
   - Submit Commands
   - Present Image
```

### Synchronization

Uses **double buffering** (MAX_FRAMES_IN_FLIGHT = 2):

- **Image Available Semaphore**: Signals when swapchain image is ready
- **Render Finished Semaphore**: Signals when rendering is complete
- **In-Flight Fence**: Ensures we don't submit more than 2 frames ahead

### Dynamic Rendering (Vulkan 1.3)

Uses modern dynamic rendering (no render passes):
- `vkCmdBeginRendering()` / `vkCmdEndRendering()`
- Direct color attachment specification
- Clear values per-attachment

## Vertex Data Structure

```cpp
struct TextVertex {
    float pos[2];  // Screen position (pixels)
    float uv[2];   // Atlas UV coordinates (0-1)
};
```

## Push Constants

```cpp
struct TextPushConstants {
    float screenSize[2];  // Width, height
    float textColor[4];   // RGBA (stored as 2 vec2s in shader)
};
```

## Building and Running

### Build Steps

1. **Configure CMake** (if needed):
   ```bash
   cmake -B build
   ```

2. **Build**:
   ```bash
   cmake --build build
   ```

3. **Run**:
   ```bash
   ./build/CMakeProject7.exe
   ```

The application will display "Hello, Vulkan Text Rendering!" on a dark blue background.

## Customization

### Change Text

In `main.cpp`, line ~96:
```cpp
auto shapedGlyphs = shaper.shape_utf8("Your text here");
```

### Change Text Position

In `main.cpp`, line ~143:
```cpp
auto textCtx = createTextRenderer(device, swapchain, "assets/fonts/EBGaramond-Regular.ttf", 48);
textRendererSetText(textCtx, "UI Label", 50.0f, 300.0f);
Buffer staging = textRendererFinalizeAtlas(textCtx, uploadCmd);
textRendererUploadVertices(textCtx);
//                                                                      X      Y
```

### Change Text Color

In `main.cpp`, lines ~227-230:
```cpp
pushConstants.textColor[0] = 1.0f; // Red
pushConstants.textColor[1] = 1.0f; // Green
pushConstants.textColor[2] = 1.0f; // Blue
pushConstants.textColor[3] = 1.0f; // Alpha
```

### Change Background Color

In `main.cpp`, line ~195:
```cpp
colorAttachment.clearValue.color = {{0.1f, 0.1f, 0.15f, 1.0f}};
//                                    R     G     B      A
```

### Change Font Size

In `main.cpp`, lines ~82-86:
```cpp
atlas.loadFont("assets/fonts/EBGaramond-Regular.ttf", 64);  // Size in pixels
HbShaper shaper("assets/fonts/EBGaramond-Regular.ttf", 64);
```

## Key Features

### Swapchain
- Automatic format selection (prefers B8G8R8A8_SRGB)
- Mailbox present mode (if available) for low latency
- Handles VK_ERROR_OUT_OF_DATE_KHR gracefully
- Window resize support

### Pipeline
- Alpha blending for smooth text edges
- Dynamic viewport and scissor
- No depth testing (2D rendering)
- Backface culling disabled (for flexibility)

### Performance
- Vertices uploaded once (static text)
- Atlas texture stays on GPU
- Minimal state changes per frame
- Efficient double buffering

## Troubleshooting

### "Failed to create instance"
- Ensure Vulkan 1.3 drivers are installed
- Check that GLFW extensions are supported

### "Failed to create swap chain"
- Window may be minimized
- Graphics drivers may need updating

### Black screen
- Verify `shaders/` directory contains .spv files
- Check that text is within window bounds
- Verify atlas was finalized

### Text not appearing
- Check that glyphs were added to atlas before finalizing
- Verify vertex buffer contains data
- Check text color alpha isn't 0

## Next Steps

### Multiple Text Strings
Build multiple vertex buffers or combine into one with offset draws.

### Dynamic Text
Update vertex buffer contents each frame using mapped memory.

### Text Effects
- Modify shaders for drop shadows, outlines
- Add distance field rendering for scalable text
- Implement text animations in vertex shader

### Performance Optimization
- Batch multiple text draws
- Use indirect drawing for many strings
- Implement text caching and culling

## Technical Notes

### Coordinate System
- Origin (0, 0) is top-left
- X increases right
- Y increases down
- Matches typical 2D UI conventions

### UV Coordinates
- Automatically calculated by GlyphAtlas
- Account for glyph metrics (bearing, advance)
- Include padding to prevent bleeding

### Memory Management
- VMA handles all allocations
- Staging buffers used for transfers
- Automatic cleanup in destructors

## References

- Vulkan 1.3 Specification
- VMA (Vulkan Memory Allocator) documentation
- FreeType 2 documentation
- HarfBuzz text shaping library
- GLFW window management

