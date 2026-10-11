#pragma once

// A small CPU rasteriser for Dear ImGui draw data.
//
// The VST3 editor renders into a plain 32-bit pixel buffer that each platform
// then blits into its window. That avoids OpenGL contexts inside a host's
// process (a classic source of plug-in crashes and driver quirks) and makes
// the editor behave identically on Windows, macOS and Linux.
//
// Pixel format: 0xAARRGGBB in a uint32_t (B,G,R,A byte order in memory on
// little-endian machines, which is what GDI, X11 TrueColor and CoreGraphics'
// kCGBitmapByteOrder32Little|PremultipliedFirst all expect).

#include "imgui.h"
#include "pixel_rect.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace sdl2synth {

class SwRenderer {
public:
  SwRenderer();
  ~SwRenderer();

  // Must be called every frame before render(): services the texture
  // create/update/destroy requests ImGui queues for the font atlas.
  void updateTextures(ImDrawData* drawData);

  // Rasterises `drawData` into `pixels` (width*height, tightly packed). Only
  // the 64x64 tiles whose contents changed since the previous call are
  // redrawn (everything when `forceAll` is set or the size changed). Returns
  // the changed regions, which is empty when the image is identical.
  const std::vector<PixelRect>& render(const ImDrawData* drawData, uint32_t* pixels, int width, int height,
                                       uint32_t clearColor, bool forceAll);

private:
  struct Texture {
    int width = 0;
    int height = 0;
    int bytesPerPixel = 4;
    bool colored = false; // false: texels are white, only alpha varies
    std::vector<uint8_t> data;
  };
  struct Target;
  struct Frame;

  std::vector<std::unique_ptr<Texture>> textures_;
  uint64_t textureVersion_ = 0; // bumped whenever a texture's contents change

  // Change tracking between frames.
  std::unique_ptr<Frame> frame_;
  std::vector<uint64_t> previousHashes_;
  std::vector<PixelRect> dirty_;
  int lastWidth_ = 0, lastHeight_ = 0;
};

} // namespace sdl2synth
