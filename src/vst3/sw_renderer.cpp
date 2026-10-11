#include "sw_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sdl2synth {

namespace {

// Sub-pixel precision used for rasterisation: coordinates are stored as 24.8
// fixed point so the edge functions are exact integers and neighbouring
// triangles never overlap or leave cracks (top-left fill rule).
constexpr int kSubBits = 8;
constexpr int32_t kSub = 1 << kSubBits;
constexpr int32_t kHalf = kSub / 2;

struct Vtx {
  int32_t x, y; // 24.8 fixed point, framebuffer space
  float u, v;
  float r, g, b, a; // 0..255, straight alpha
  uint32_t col;     // packed colour, kept for hashing
};

constexpr int kTileSize = 64; // pixels; the unit of change tracking

inline uint64_t mix64(uint64_t h, uint64_t v) {
  h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
  return h * 0x100000001B3ull;
}

// Exact-enough rounding division by 255 for 0..65025.
inline int div255(int x) { return (x + 128 + ((x + 128) >> 8)) >> 8; }

inline uint32_t blendPixel(uint32_t dst, int sr, int sg, int sb, int sa) {
  if (sa >= 255)
    return 0xFF000000u | ((uint32_t)sr << 16) | ((uint32_t)sg << 8) | (uint32_t)sb;
  if (sa <= 0)
    return dst;
  const int ia = 255 - sa;
  const int dr = (int)((dst >> 16) & 255u);
  const int dg = (int)((dst >> 8) & 255u);
  const int db = (int)(dst & 255u);
  const int r = div255(sr * sa + dr * ia);
  const int g = div255(sg * sa + dg * ia);
  const int b = div255(sb * sa + db * ia);
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline int32_t toFixed(float v) { return (int32_t)std::lround(v * (float)kSub); }

inline bool topLeft(int64_t dx, int64_t dy) { return dy < 0 || (dy == 0 && dx > 0); }

} // namespace

struct SwRenderer::Target {
  uint32_t* px = nullptr;
  int w = 0, h = 0;
  int clipX0 = 0, clipY0 = 0, clipX1 = 0, clipY1 = 0; // exclusive upper bounds
  const Texture* tex = nullptr;
  std::vector<int> columns; // scratch: texel column for each destination pixel

  // Samples the texture (nearest). Returns RGBA 0..255.
  inline void sample(float u, float v, int& r, int& g, int& b, int& a) const {
    if (!tex || tex->data.empty()) {
      r = g = b = a = 255;
      return;
    }
    const int tx = clampi((int)(u * (float)tex->width), 0, tex->width - 1);
    const int ty = clampi((int)(v * (float)tex->height), 0, tex->height - 1);
    if (tex->bytesPerPixel == 1) {
      r = g = b = 255;
      a = tex->data[(size_t)ty * (size_t)tex->width + (size_t)tx];
    } else {
      const uint8_t* p = &tex->data[((size_t)ty * (size_t)tex->width + (size_t)tx) * 4];
      r = p[0];
      g = p[1];
      b = p[2];
      a = p[3];
    }
  }

  // Writes `count` pixels of one constant colour (alpha < 255 blends).
  static inline void spanSolid(uint32_t* row, int count, int sr, int sg, int sb, int sa) {
    if (sa >= 255) {
      const uint32_t c = 0xFF000000u | ((uint32_t)sr << 16) | ((uint32_t)sg << 8) | (uint32_t)sb;
      for (int i = 0; i < count; ++i)
        row[i] = c;
    } else if (sa > 0) {
      const int ia = 255 - sa;
      const int kr = sr * sa, kg = sg * sa, kb = sb * sa;
      for (int i = 0; i < count; ++i) {
        const uint32_t d = row[i];
        const int r = div255(kr + (int)((d >> 16) & 255u) * ia);
        const int g = div255(kg + (int)((d >> 8) & 255u) * ia);
        const int b = div255(kb + (int)(d & 255u) * ia);
        row[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
      }
    }
  }

  // Axis-aligned rectangle with optional texture mapping. Coordinates are
  // fixed point; pixel centres inside [x0,x1) x [y0,y1) are covered.
  void fillRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float u0, float v0, float u1, float v1,
                float cr, float cg, float cb, float ca, bool constUv) {
    if (x1 <= x0 || y1 <= y0)
      return;
    const int px0 = std::max(clipX0, (int)((x0 - kHalf + (kSub - 1)) >> kSubBits));
    const int px1 = std::min(clipX1, (int)((x1 - kHalf + (kSub - 1)) >> kSubBits));
    const int py0 = std::max(clipY0, (int)((y0 - kHalf + (kSub - 1)) >> kSubBits));
    const int py1 = std::min(clipY1, (int)((y1 - kHalf + (kSub - 1)) >> kSubBits));
    if (px0 >= px1 || py0 >= py1)
      return;

    const int cri = (int)(cr + 0.5f), cgi = (int)(cg + 0.5f), cbi = (int)(cb + 0.5f), cai = (int)(ca + 0.5f);

    if (constUv) {
      int tr, tg, tb, ta;
      sample(u0, v0, tr, tg, tb, ta);
      const int sr = div255(cri * tr), sg = div255(cgi * tg), sb = div255(cbi * tb), sa = div255(cai * ta);
      if (sa <= 0)
        return;
      for (int y = py0; y < py1; ++y)
        spanSolid(px + (size_t)y * (size_t)w + px0, px1 - px0, sr, sg, sb, sa);
      return;
    }

    if (!tex || tex->data.empty())
      return;

    // Textured quad (glyphs): resolve the texel column of every destination
    // pixel once, then the inner loop is plain table lookups.
    const float invW = 1.0f / (float)(x1 - x0);
    const float invH = 1.0f / (float)(y1 - y0);
    columns.resize((size_t)(px1 - px0));
    for (int x = px0; x < px1; ++x) {
      const float u = u0 + (float)((x << kSubBits) + kHalf - x0) * invW * (u1 - u0);
      columns[(size_t)(x - px0)] = clampi((int)(u * (float)tex->width), 0, tex->width - 1);
    }
    const int bpp = tex->bytesPerPixel;
    const bool white = !tex->colored; // texels are white with an alpha channel
    for (int y = py0; y < py1; ++y) {
      const float v = v0 + (float)((y << kSubBits) + kHalf - y0) * invH * (v1 - v0);
      const int ty = clampi((int)(v * (float)tex->height), 0, tex->height - 1);
      const uint8_t* src = &tex->data[(size_t)ty * (size_t)tex->width * (size_t)bpp];
      uint32_t* row = px + (size_t)y * (size_t)w;
      for (int x = px0; x < px1; ++x) {
        const uint8_t* t = src + (size_t)columns[(size_t)(x - px0)] * (size_t)bpp;
        const int ta = bpp == 1 ? t[0] : t[3];
        if (ta == 0)
          continue;
        const int sa = cai >= 255 ? ta : div255(cai * ta);
        if (white || bpp == 1)
          row[x] = blendPixel(row[x], cri, cgi, cbi, sa);
        else
          row[x] = blendPixel(row[x], div255(cri * t[0]), div255(cgi * t[1]), div255(cbi * t[2]), sa);
      }
    }
  }

  void fillTriangle(const Vtx& a, Vtx b, Vtx c) {
    int64_t area = (int64_t)(b.x - a.x) * (c.y - a.y) - (int64_t)(b.y - a.y) * (c.x - a.x);
    if (area == 0)
      return;
    if (area < 0) {
      std::swap(b, c);
      area = -area;
    }

    const int32_t minX = std::min(a.x, std::min(b.x, c.x));
    const int32_t maxX = std::max(a.x, std::max(b.x, c.x));
    const int32_t minY = std::min(a.y, std::min(b.y, c.y));
    const int32_t maxY = std::max(a.y, std::max(b.y, c.y));
    const int px0 = std::max(clipX0, (int)(minX >> kSubBits));
    const int px1 = std::min(clipX1, (int)((maxX + kSub - 1) >> kSubBits));
    const int py0 = std::max(clipY0, (int)(minY >> kSubBits));
    const int py1 = std::min(clipY1, (int)((maxY + kSub - 1) >> kSubBits));
    if (px0 >= px1 || py0 >= py1)
      return;

    // Edge functions: e0 = b->c (weight of a), e1 = c->a (weight of b), e2 = a->b (weight of c).
    int64_t row[3], stepX[3], stepY[3];
    const Vtx* from[3] = {&b, &c, &a};
    const Vtx* to[3] = {&c, &a, &b};
    const int64_t startX = (int64_t)px0 * kSub + kHalf;
    const int64_t startY = (int64_t)py0 * kSub + kHalf;
    for (int i = 0; i < 3; ++i) {
      const int64_t dx = to[i]->x - from[i]->x;
      const int64_t dy = to[i]->y - from[i]->y;
      row[i] = dx * (startY - from[i]->y) - dy * (startX - from[i]->x);
      if (!topLeft(dx, dy))
        row[i] -= 1; // exclude pixels centred exactly on a right/bottom edge
      stepX[i] = -dy * kSub;
      stepY[i] = dx * kSub;
    }

    const bool flat = a.r == b.r && a.r == c.r && a.g == b.g && a.g == c.g && a.b == b.b && a.b == c.b &&
                      a.a == b.a && a.a == c.a;
    const bool constUv = a.u == b.u && a.u == c.u && a.v == b.v && a.v == c.v;
    const float invArea = 1.0f / (float)area;

    // Flat, untextured-or-constant-texture fills use one precomputed colour.
    int fr = 0, fg = 0, fb = 0, fa = 0;
    if (flat && constUv) {
      int tr, tg, tb, ta;
      sample(a.u, a.v, tr, tg, tb, ta);
      fr = div255((int)(a.r + 0.5f) * tr);
      fg = div255((int)(a.g + 0.5f) * tg);
      fb = div255((int)(a.b + 0.5f) * tb);
      fa = div255((int)(a.a + 0.5f) * ta);
      if (fa <= 0)
        return;
    }
    const bool simple = flat && constUv;

    // Per-pixel gradients of the interpolated attributes (weights are linear
    // in x and y, so every attribute is too).
    const float dl0 = (float)stepX[0] * invArea;
    const float dl1 = (float)stepX[1] * invArea;
    const float gr = (a.r - c.r) * dl0 + (b.r - c.r) * dl1;
    const float gg = (a.g - c.g) * dl0 + (b.g - c.g) * dl1;
    const float gb = (a.b - c.b) * dl0 + (b.b - c.b) * dl1;
    const float ga = (a.a - c.a) * dl0 + (b.a - c.a) * dl1;
    const float gu = (a.u - c.u) * dl0 + (b.u - c.u) * dl1;
    const float gv = (a.v - c.v) * dl0 + (b.v - c.v) * dl1;

    for (int y = py0; y < py1; ++y) {
      // Solve the three edge inequalities for the covered span of this row.
      int xs = px0, xe = px1;
      for (int i = 0; i < 3 && xs < xe; ++i) {
        const int64_t e0 = row[i], s = stepX[i];
        if (s == 0) {
          if (e0 < 0)
            xs = xe;
        } else if (s > 0) {
          if (e0 < 0) {
            const int64_t n = (-e0 + s - 1) / s;
            xs = (int)std::min<int64_t>((int64_t)px0 + n, (int64_t)px1);
          }
        } else {
          if (e0 < 0) {
            xs = xe;
          } else {
            const int64_t n = e0 / (-s);
            xe = (int)std::min<int64_t>((int64_t)px0 + n + 1, (int64_t)xe);
          }
        }
        // `xs` may only grow past previous bounds; keep the larger lower bound.
      }
      if (xs < xe) {
        uint32_t* dst = px + (size_t)y * (size_t)w;
        if (simple) {
          spanSolid(dst + xs, xe - xs, fr, fg, fb, fa);
        } else {
          const int k = xs - px0;
          const float l0 = (float)(row[0] + stepX[0] * k) * invArea;
          const float l1 = (float)(row[1] + stepX[1] * k) * invArea;
          const float l2 = 1.0f - l0 - l1;
          float cr = a.r * l0 + b.r * l1 + c.r * l2;
          float cg = a.g * l0 + b.g * l1 + c.g * l2;
          float cb = a.b * l0 + b.b * l1 + c.b * l2;
          float ca = a.a * l0 + b.a * l1 + c.a * l2;
          float u = a.u * l0 + b.u * l1 + c.u * l2;
          float v = a.v * l0 + b.v * l1 + c.v * l2;
          for (int x = xs; x < xe; ++x) {
            int tr = 255, tg = 255, tb = 255, ta = 255;
            if (tex)
              sample(u, v, tr, tg, tb, ta);
            const int sa = div255(clampi((int)(ca + 0.5f), 0, 255) * ta);
            if (sa > 0) {
              const int sr = div255(clampi((int)(cr + 0.5f), 0, 255) * tr);
              const int sg = div255(clampi((int)(cg + 0.5f), 0, 255) * tg);
              const int sb = div255(clampi((int)(cb + 0.5f), 0, 255) * tb);
              dst[x] = blendPixel(dst[x], sr, sg, sb, sa);
            }
            cr += gr; cg += gg; cb += gb; ca += ga; u += gu; v += gv;
          }
        }
      }
      for (int i = 0; i < 3; ++i)
        row[i] += stepY[i];
    }
  }
};

void SwRenderer::updateTextures(ImDrawData* drawData) {
  if (!drawData || !drawData->Textures)
    return;
  for (ImTextureData* tex : *drawData->Textures) {
    switch (tex->Status) {
      case ImTextureStatus_WantCreate: {
        auto t = std::make_unique<Texture>();
        t->width = tex->Width;
        t->height = tex->Height;
        t->bytesPerPixel = tex->BytesPerPixel;
        t->colored = tex->UseColors;
        t->data.assign(tex->Pixels, tex->Pixels + tex->GetSizeInBytes());
        tex->SetTexID((ImTextureID)(uintptr_t)t.get());
        tex->BackendUserData = t.get();
        textures_.push_back(std::move(t));
        tex->SetStatus(ImTextureStatus_OK);
        ++textureVersion_;
        break;
      }
      case ImTextureStatus_WantUpdates: {
        Texture* t = static_cast<Texture*>(tex->BackendUserData);
        if (t) {
          t->colored = tex->UseColors;
          // Updates are rare and small; copy the changed rows.
          const ImTextureRect& r = tex->UpdateRect;
          const size_t pitch = (size_t)tex->Width * (size_t)tex->BytesPerPixel;
          const size_t rowBytes = (size_t)r.w * (size_t)tex->BytesPerPixel;
          for (int y = r.y; y < r.y + r.h; ++y) {
            std::memcpy(&t->data[(size_t)y * pitch + (size_t)r.x * (size_t)tex->BytesPerPixel],
                        tex->Pixels + (size_t)y * pitch + (size_t)r.x * (size_t)tex->BytesPerPixel, rowBytes);
          }
        }
        tex->SetStatus(ImTextureStatus_OK);
        ++textureVersion_;
        break;
      }
      case ImTextureStatus_WantDestroy:
        if (tex->UnusedFrames > 0) {
          Texture* t = static_cast<Texture*>(tex->BackendUserData);
          textures_.erase(std::remove_if(textures_.begin(), textures_.end(),
                                         [t](const std::unique_ptr<Texture>& p) { return p.get() == t; }),
                          textures_.end());
          tex->SetTexID(ImTextureID_Invalid);
          tex->BackendUserData = nullptr;
          tex->SetStatus(ImTextureStatus_Destroyed);
          ++textureVersion_;
        }
        break;
      default:
        break;
    }
  }
}

namespace {

struct Cmd {
  int clipX0, clipY0, clipX1, clipY1;
  const void* tex;
  uint64_t hash;
};

struct PrimRef {
  uint32_t cmd;        // index into the frame's command table
  uint32_t vtx[4];     // indices into the frame's vertex table
  bool quad;
};

} // namespace

struct SwRenderer::Frame {
  std::vector<Vtx> verts;
  std::vector<Cmd> cmds;
  std::vector<PrimRef> prims;
  std::vector<std::vector<uint32_t>> bins; // per tile: indices into prims, in draw order
  std::vector<uint64_t> hashes;            // per tile: hash of everything drawn into it
};

SwRenderer::SwRenderer() : frame_(new Frame) {}
SwRenderer::~SwRenderer() = default;

const std::vector<PixelRect>& SwRenderer::render(const ImDrawData* dd, uint32_t* pixels, int width, int height,
                                                 uint32_t clearColor, bool forceAll) {
  dirty_.clear();
  if (!dd || width <= 0 || height <= 0)
    return dirty_;

  Frame& f = *frame_;
  const int tilesX = (width + kTileSize - 1) / kTileSize;
  const int tilesY = (height + kTileSize - 1) / kTileSize;
  const size_t tileCount = (size_t)tilesX * (size_t)tilesY;
  if (width != lastWidth_ || height != lastHeight_) {
    lastWidth_ = width;
    lastHeight_ = height;
    previousHashes_.assign(tileCount, 0);
    forceAll = true;
  }
  f.verts.clear();
  f.cmds.clear();
  f.prims.clear();
  if (f.bins.size() < tileCount)
    f.bins.resize(tileCount);
  for (size_t i = 0; i < tileCount; ++i)
    f.bins[i].clear();
  f.hashes.assign(tileCount, 0x517CC1B727220A95ull ^ textureVersion_);

  const ImVec2 clipOff = dd->DisplayPos;
  const ImVec2 scale = dd->FramebufferScale;

  // --- Pass 1: convert, hash and bin every primitive into the tiles it touches.
  for (int n = 0; n < dd->CmdListsCount; ++n) {
    const ImDrawList* cl = dd->CmdLists[n];
    const ImDrawVert* vtxBuffer = cl->VtxBuffer.Data;
    const ImDrawIdx* idxBuffer = cl->IdxBuffer.Data;

    const uint32_t vtxBase = (uint32_t)f.verts.size();
    f.verts.resize(f.verts.size() + (size_t)cl->VtxBuffer.Size);
    for (int i = 0; i < cl->VtxBuffer.Size; ++i) {
      const ImDrawVert& s = vtxBuffer[i];
      Vtx& d = f.verts[vtxBase + (uint32_t)i];
      d.x = toFixed((s.pos.x - clipOff.x) * scale.x);
      d.y = toFixed((s.pos.y - clipOff.y) * scale.y);
      d.u = s.uv.x;
      d.v = s.uv.y;
      d.col = s.col;
      d.r = (float)((s.col >> IM_COL32_R_SHIFT) & 0xFF);
      d.g = (float)((s.col >> IM_COL32_G_SHIFT) & 0xFF);
      d.b = (float)((s.col >> IM_COL32_B_SHIFT) & 0xFF);
      d.a = (float)((s.col >> IM_COL32_A_SHIFT) & 0xFF);
    }

    for (const ImDrawCmd& cmd : cl->CmdBuffer) {
      if (cmd.UserCallback || cmd.ElemCount == 0)
        continue;
      Cmd c;
      c.clipX0 = clampi((int)std::floor((cmd.ClipRect.x - clipOff.x) * scale.x), 0, width);
      c.clipY0 = clampi((int)std::floor((cmd.ClipRect.y - clipOff.y) * scale.y), 0, height);
      c.clipX1 = clampi((int)std::ceil((cmd.ClipRect.z - clipOff.x) * scale.x), 0, width);
      c.clipY1 = clampi((int)std::ceil((cmd.ClipRect.w - clipOff.y) * scale.y), 0, height);
      if (c.clipX0 >= c.clipX1 || c.clipY0 >= c.clipY1)
        continue;
      const ImTextureID texId = cmd.GetTexID();
      c.tex = texId == ImTextureID_Invalid ? nullptr : reinterpret_cast<const void*>((uintptr_t)texId);
      c.hash = mix64(mix64(mix64((uint64_t)c.clipX0, (uint64_t)c.clipY0), mix64((uint64_t)c.clipX1, (uint64_t)c.clipY1)),
                     (uint64_t)(uintptr_t)c.tex);
      const uint32_t cmdIndex = (uint32_t)f.cmds.size();
      f.cmds.push_back(c);

      const ImDrawIdx* idx = idxBuffer + cmd.IdxOffset;
      const uint32_t base = vtxBase + cmd.VtxOffset;
      const unsigned count = cmd.ElemCount;

      for (unsigned i = 0; i + 3 <= count;) {
        PrimRef p;
        p.cmd = cmdIndex;
        p.quad = false;
        // Rectangles (UI boxes, glyph quads) arrive as two triangles sharing
        // a diagonal: (a,b,c) (a,c,d). Treat them as one axis-aligned fill.
        if (i + 6 <= count && idx[i] == idx[i + 3] && idx[i + 2] == idx[i + 4]) {
          const Vtx& a = f.verts[base + idx[i]];
          const Vtx& b = f.verts[base + idx[i + 1]];
          const Vtx& c2 = f.verts[base + idx[i + 2]];
          const Vtx& d = f.verts[base + idx[i + 5]];
          const bool aligned = a.y == b.y && b.x == c2.x && c2.y == d.y && d.x == a.x && a.col == b.col &&
                               a.col == c2.col && a.col == d.col;
          if (aligned) {
            p.quad = true;
            p.vtx[0] = base + idx[i];
            p.vtx[1] = base + idx[i + 1];
            p.vtx[2] = base + idx[i + 2];
            p.vtx[3] = base + idx[i + 5];
            i += 6;
          }
        }
        if (!p.quad) {
          p.vtx[0] = base + idx[i];
          p.vtx[1] = base + idx[i + 1];
          p.vtx[2] = base + idx[i + 2];
          p.vtx[3] = p.vtx[2];
          i += 3;
        }

        // Pixel bounding box of the primitive, limited by its clip rectangle.
        int32_t minX = INT32_MAX, minY = INT32_MAX, maxX = INT32_MIN, maxY = INT32_MIN;
        uint64_t h = c.hash;
        const int vcount = p.quad ? 4 : 3;
        for (int k = 0; k < vcount; ++k) {
          const Vtx& v = f.verts[p.vtx[k]];
          minX = std::min(minX, v.x);
          maxX = std::max(maxX, v.x);
          minY = std::min(minY, v.y);
          maxY = std::max(maxY, v.y);
          uint32_t ub, vb;
          std::memcpy(&ub, &v.u, 4);
          std::memcpy(&vb, &v.v, 4);
          h = mix64(h, ((uint64_t)(uint32_t)v.x << 32) | (uint32_t)v.y);
          h = mix64(h, ((uint64_t)ub << 32) | vb);
          h = mix64(h, v.col);
        }
        h = mix64(h, p.quad ? 1 : 2);
        const int bx0 = std::max(c.clipX0, (int)(minX >> kSubBits));
        const int by0 = std::max(c.clipY0, (int)(minY >> kSubBits));
        const int bx1 = std::min(c.clipX1, (int)((maxX + kSub - 1) >> kSubBits));
        const int by1 = std::min(c.clipY1, (int)((maxY + kSub - 1) >> kSubBits));
        if (bx0 >= bx1 || by0 >= by1)
          continue;

        const uint32_t primIndex = (uint32_t)f.prims.size();
        f.prims.push_back(p);
        const int tx0 = bx0 / kTileSize, tx1 = (bx1 - 1) / kTileSize;
        const int ty0 = by0 / kTileSize, ty1 = (by1 - 1) / kTileSize;
        for (int ty = ty0; ty <= ty1; ++ty) {
          for (int tx = tx0; tx <= tx1; ++tx) {
            const size_t tile = (size_t)ty * (size_t)tilesX + (size_t)tx;
            f.bins[tile].push_back(primIndex);
            f.hashes[tile] = mix64(f.hashes[tile], h);
          }
        }
      }
    }
  }

  // --- Pass 2: re-render the tiles whose contents changed.
  Target target;
  target.px = pixels;
  target.w = width;
  target.h = height;
  std::vector<PixelRect> runs;
  for (int ty = 0; ty < tilesY; ++ty) {
    int runStart = -1;
    for (int tx = 0; tx <= tilesX; ++tx) {
      bool changed = false;
      if (tx < tilesX) {
        const size_t tile = (size_t)ty * (size_t)tilesX + (size_t)tx;
        changed = forceAll || f.hashes[tile] != previousHashes_[tile];
        if (changed) {
          const int x0 = tx * kTileSize, y0 = ty * kTileSize;
          const int x1 = std::min(x0 + kTileSize, width), y1 = std::min(y0 + kTileSize, height);
          for (int y = y0; y < y1; ++y)
            std::fill(pixels + (size_t)y * (size_t)width + x0, pixels + (size_t)y * (size_t)width + x1, clearColor);
          for (uint32_t primIndex : f.bins[tile]) {
            const PrimRef& p = f.prims[primIndex];
            const Cmd& c = f.cmds[p.cmd];
            target.clipX0 = std::max(c.clipX0, x0);
            target.clipY0 = std::max(c.clipY0, y0);
            target.clipX1 = std::min(c.clipX1, x1);
            target.clipY1 = std::min(c.clipY1, y1);
            if (target.clipX0 >= target.clipX1 || target.clipY0 >= target.clipY1)
              continue;
            target.tex = c.tex ? reinterpret_cast<const Texture*>(c.tex) : nullptr;
            if (p.quad) {
              const Vtx& a = f.verts[p.vtx[0]];
              const Vtx& b = f.verts[p.vtx[1]];
              const Vtx& cc = f.verts[p.vtx[2]];
              const int32_t qx0 = std::min(a.x, cc.x), qx1 = std::max(a.x, cc.x);
              const int32_t qy0 = std::min(a.y, cc.y), qy1 = std::max(a.y, cc.y);
              const bool constUv = a.u == cc.u && a.v == cc.v && a.u == b.u && a.v == b.v;
              float u0 = a.u, v0 = a.v, u1 = cc.u, v1 = cc.v;
              if (a.x > cc.x) std::swap(u0, u1);
              if (a.y > cc.y) std::swap(v0, v1);
              target.fillRect(qx0, qy0, qx1, qy1, u0, v0, u1, v1, a.r, a.g, a.b, a.a, constUv);
            } else {
              target.fillTriangle(f.verts[p.vtx[0]], f.verts[p.vtx[1]], f.verts[p.vtx[2]]);
            }
          }
        }
      }
      if (changed && runStart < 0)
        runStart = tx;
      if (!changed && runStart >= 0) {
        const int x0 = runStart * kTileSize, y0 = ty * kTileSize;
        runs.push_back({x0, y0, std::min(tx * kTileSize, width) - x0, std::min(y0 + kTileSize, height) - y0});
        runStart = -1;
      }
    }
  }

  previousHashes_ = f.hashes;
  dirty_ = std::move(runs);
  return dirty_;
}

} // namespace sdl2synth
