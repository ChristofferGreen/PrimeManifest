#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/text/FontBitmap.hpp"

#include "PrimeManifest/util/BitmapFont.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <unordered_map>

#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H
#include <hb.h>
#include <hb-ft.h>
#endif

namespace PrimeManifest {

#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
namespace {

struct Utf8Codepoint {
  uint32_t codepoint = 0;
  size_t byteOffset = 0;
  size_t byteLength = 0;
};

auto decode_utf8(std::string_view text) -> std::vector<Utf8Codepoint> {
  std::vector<Utf8Codepoint> out;
  size_t i = 0;
  while (i < text.size()) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    Utf8Codepoint cp;
    cp.byteOffset = i;
    if (c < 0x80) {
      cp.codepoint = c;
      cp.byteLength = 1;
      i += 1;
    } else if ((c >> 5) == 0x6 && i + 1 < text.size()) {
      cp.codepoint = ((c & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
      cp.byteLength = 2;
      i += 2;
    } else if ((c >> 4) == 0xE && i + 2 < text.size()) {
      cp.codepoint = ((c & 0x0F) << 12) |
                     ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                     (static_cast<unsigned char>(text[i + 2]) & 0x3F);
      cp.byteLength = 3;
      i += 3;
    } else if ((c >> 3) == 0x1E && i + 3 < text.size()) {
      cp.codepoint = ((c & 0x07) << 18) |
                     ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
                     ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
                     (static_cast<unsigned char>(text[i + 3]) & 0x3F);
      cp.byteLength = 4;
      i += 4;
    } else {
      cp.codepoint = 0xFFFD;
      cp.byteLength = 1;
      i += 1;
    }
    out.push_back(cp);
  }
  return out;
}

struct FontFace {
  uint32_t id = 0;
  std::string family;
  uint16_t weight = 400;
  FontSlant slant = FontSlant::Upright;
  FT_Face face = nullptr;
  hb_font_t* hbFont = nullptr;
  bool fromBundle = false;
};

struct FontBuffer {
  std::string name;
  std::vector<uint8_t> data;
};

struct GlyphKey {
  uint32_t faceId = 0;
  uint16_t sizePx = 0;
  uint16_t embolden = 0;
  uint32_t glyphId = 0;

  bool operator==(GlyphKey const& other) const {
    return faceId == other.faceId && sizePx == other.sizePx && embolden == other.embolden && glyphId == other.glyphId;
  }
};

struct GlyphKeyHash {
  size_t operator()(GlyphKey const& key) const {
    size_t h = static_cast<size_t>(key.faceId);
    h = (h * 1315423911u) ^ static_cast<size_t>(key.sizePx + 0x9e3779b9);
    h = (h * 2654435761u) ^ static_cast<size_t>(key.embolden + 0x85ebca6b);
    h = (h * 2246822519u) ^ static_cast<size_t>(key.glyphId + 0x7f4a7c15);
    return h;
  }
};

static auto fnv1a_hash(uint64_t h, uint64_t v) -> uint64_t {
  constexpr uint64_t Prime = 1099511628211ull;
  h ^= v;
  h *= Prime;
  return h;
}

static auto to_lower(std::string_view text) -> std::string {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

static auto compute_synthetic_bold(uint16_t faceWeight,
                                   uint16_t targetWeight,
                                   uint16_t sizePx) -> uint16_t {
  if (sizePx == 0u || targetWeight <= faceWeight) return 0u;
  float weightDelta = std::min<float>(static_cast<float>(targetWeight - faceWeight), 300.0f);
  float weightScale = weightDelta / 300.0f;
  constexpr float EmboldenScale = 0.04f;
  constexpr float EmboldenMinPx = 0.25f;
  constexpr float EmboldenMaxPx = 1.5f;
  float pxStrength = static_cast<float>(sizePx) * EmboldenScale * weightScale;
  if (pxStrength < EmboldenMinPx) return 0u;
  pxStrength = std::min(pxStrength, EmboldenMaxPx);
  return static_cast<uint16_t>(std::lround(pxStrength * 64.0f));
}

static auto resolve_face_weight(FT_Face face) -> uint16_t {
  if (!face) return 400;
  if (auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, ft_sfnt_os2)); os2) {
    if (os2->usWeightClass != 0) {
      uint32_t weight = std::min<uint32_t>(os2->usWeightClass, 1000u);
      return static_cast<uint16_t>(std::max<uint32_t>(weight, 1u));
    }
  }
  return (face->style_flags & FT_STYLE_FLAG_BOLD) ? 700 : 400;
}

static auto infer_weight_from_style(std::string_view style) -> std::optional<uint16_t> {
  if (style.empty()) return std::nullopt;
  auto lowered = to_lower(style);
  auto has = [&](std::string_view needle) { return lowered.find(needle) != std::string::npos; };

  if (has("thin")) return 100;
  if (has("extralight") || has("ultralight")) return 200;
  if (has("light")) return 300;
  if (has("regular") || has("normal") || has("book")) return 400;
  if (has("medium")) return 500;
  if (has("semibold") || has("demibold")) return 600;
  if (has("extrabold") || has("ultrabold")) return 800;
  if (has("black") || has("heavy")) return 900;
  if (has("bold")) return 700;
  return std::nullopt;
}

static auto is_word_space(uint32_t codepoint) -> bool {
  switch (codepoint) {
    case 0x20u: // space
    case 0xA0u: // no-break space
    case 0x2002u: // en space
    case 0x2003u: // em space
    case 0x2009u: // thin space
    case 0x200Au: // hair space
      return true;
    default:
      return false;
  }
}

static bool is_font_file(std::filesystem::path const& path) {
  auto ext = to_lower(path.extension().string());
  return ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".otc";
}

static bool is_bundle_file(std::filesystem::path const& path) {
  auto ext = to_lower(path.extension().string());
  return ext == ".psfont";
}

static void append_font_files(std::filesystem::path const& root,
                              std::vector<std::string>& out) {
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) return;
  for (auto const& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    if (!is_font_file(entry.path())) continue;
    out.push_back(entry.path().string());
  }
}

static void append_bundle_files(std::filesystem::path const& root,
                                std::vector<std::string>& out) {
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) return;
  for (auto const& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    if (!is_bundle_file(entry.path())) continue;
    out.push_back(entry.path().string());
  }
}

static auto parse_features(std::string const& features) -> std::vector<hb_feature_t> {
  std::vector<hb_feature_t> out;
  if (features.empty()) return out;
  size_t start = 0;
  while (start < features.size()) {
    size_t end = features.find(',', start);
    if (end == std::string::npos) end = features.size();
    hb_feature_t feature;
    if (hb_feature_from_string(features.data() + start, static_cast<int>(end - start), &feature)) {
      out.push_back(feature);
    }
    start = end + 1;
  }
  return out;
}

static bool face_supports_glyph(FontFace* face, uint32_t codepoint) {
  if (!face || !face->face) return false;
  return FT_Get_Char_Index(face->face, codepoint) != 0;
}

static void select_unicode_charmap(FT_Face face) {
  if (!face) return;
  if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0) return;
  for (int i = 0; i < face->num_charmaps; ++i) {
    if (face->charmaps[i] && face->charmaps[i]->encoding == FT_ENCODING_UNICODE) {
      FT_Set_Charmap(face, face->charmaps[i]);
      break;
    }
  }
}

static uint16_t set_face_pixel_size(FT_Face face, uint16_t sizePx) {
  if (!face || sizePx == 0) return 0;
  if (FT_Set_Pixel_Sizes(face, 0, sizePx) == 0) return sizePx;
  if (face->num_fixed_sizes <= 0) return 0;

  int bestIndex = -1;
  int bestDiff = std::numeric_limits<int>::max();
  int bestSize = 0;
  for (int i = 0; i < face->num_fixed_sizes; ++i) {
    FT_Bitmap_Size size = face->available_sizes[i];
    int yPpem = size.y_ppem > 0 ? static_cast<int>(size.y_ppem / 64) : size.height;
    int diff = std::abs(yPpem - static_cast<int>(sizePx));
    if (diff < bestDiff || (diff == bestDiff && yPpem > bestSize)) {
      bestDiff = diff;
      bestIndex = i;
      bestSize = yPpem;
    }
  }
  if (bestIndex < 0) return 0;
  if (FT_Select_Size(face, bestIndex) != 0) return 0;
  return static_cast<uint16_t>(bestSize > 0 ? bestSize : sizePx);
}

} // namespace

struct FontRegistry::Impl {
  FT_Library ftLibrary = nullptr;
  uint32_t nextFaceId = 1;
  std::vector<std::unique_ptr<FontFace>> faces;
  std::vector<FontFace*> bundledFaces;
  std::vector<FontFace*> osFaces;
  std::unordered_map<GlyphKey, std::unique_ptr<GlyphBitmap>, GlyphKeyHash> glyphCache;
  std::unordered_map<uint64_t, FontFace*> fallbackCache;
  std::vector<std::shared_ptr<GlyphAtlas>> atlases;
  std::vector<FontBuffer> bundleBuffers;
  std::vector<std::string> bundleDirs;
  std::vector<std::string> osFontDirs;
  std::vector<std::string> osFontFiles;
  size_t osFontNext = 0;
  bool bundlesLoaded = false;
  bool osFilesLoaded = false;
  bool atlasMaxInitialized = false;
  size_t atlasMax = 0;
  std::mutex mutex;

  static constexpr int AtlasWidth = 1024;
  static constexpr int AtlasHeight = 1024;

  Impl() { FT_Init_FreeType(&ftLibrary); }
  ~Impl() {
    for (auto &face : faces) {
      if (face->hbFont) hb_font_destroy(face->hbFont);
      if (face->face) FT_Done_Face(face->face);
    }
    if (ftLibrary) FT_Done_FreeType(ftLibrary);
  }

  void loadFaceFile(std::string const& path, bool fromBundle) {
    FT_Face face = nullptr;
    if (FT_New_Face(ftLibrary, path.c_str(), 0, &face) != 0 || !face) {
      return;
    }
    int faceCount = face->num_faces;
    FT_Done_Face(face);
    for (int idx = 0; idx < std::max(1, faceCount); ++idx) {
      FT_Face f = nullptr;
      if (FT_New_Face(ftLibrary, path.c_str(), idx, &f) != 0 || !f) {
        continue;
      }
      select_unicode_charmap(f);
      auto entry = std::make_unique<FontFace>();
      entry->id = nextFaceId++;
      entry->family = f->family_name ? f->family_name : "";
      entry->weight = resolve_face_weight(f);
      if (auto styleWeight = infer_weight_from_style(f->style_name ? f->style_name : "")) {
        if (entry->weight == 0 || entry->weight == 400 ||
            std::abs(static_cast<int>(entry->weight) - static_cast<int>(*styleWeight)) >= 100) {
          entry->weight = *styleWeight;
        }
      }
      entry->slant = (f->style_flags & FT_STYLE_FLAG_ITALIC) ? FontSlant::Italic : FontSlant::Upright;
      entry->face = f;
      entry->hbFont = hb_ft_font_create_referenced(f);
      entry->fromBundle = fromBundle;

      FontFace* ptr = entry.get();
      faces.push_back(std::move(entry));
      if (fromBundle) {
        bundledFaces.push_back(ptr);
      } else {
        osFaces.push_back(ptr);
      }
    }
  }

  void loadFaceMemory(FontBuffer const& buffer, bool fromBundle) {
    if (buffer.data.empty()) return;
    FT_Face face = nullptr;
    if (FT_New_Memory_Face(ftLibrary,
                           reinterpret_cast<const FT_Byte*>(buffer.data.data()),
                           static_cast<FT_Long>(buffer.data.size()),
                           0,
                           &face) != 0 || !face) {
      return;
    }
    int faceCount = face->num_faces;
    FT_Done_Face(face);
    for (int idx = 0; idx < std::max(1, faceCount); ++idx) {
      FT_Face f = nullptr;
      if (FT_New_Memory_Face(ftLibrary,
                             reinterpret_cast<const FT_Byte*>(buffer.data.data()),
                             static_cast<FT_Long>(buffer.data.size()),
                             idx,
                             &f) != 0 || !f) {
        continue;
      }
      select_unicode_charmap(f);
      auto entry = std::make_unique<FontFace>();
      entry->id = nextFaceId++;
      entry->family = f->family_name ? f->family_name : buffer.name;
      entry->weight = resolve_face_weight(f);
      if (auto styleWeight = infer_weight_from_style(f->style_name ? f->style_name : "")) {
        if (entry->weight == 0 || entry->weight == 400 ||
            std::abs(static_cast<int>(entry->weight) - static_cast<int>(*styleWeight)) >= 100) {
          entry->weight = *styleWeight;
        }
      }
      entry->slant = (f->style_flags & FT_STYLE_FLAG_ITALIC) ? FontSlant::Italic : FontSlant::Upright;
      entry->face = f;
      entry->hbFont = hb_ft_font_create_referenced(f);
      entry->fromBundle = fromBundle;

      FontFace* ptr = entry.get();
      faces.push_back(std::move(entry));
      if (fromBundle) {
        bundledFaces.push_back(ptr);
      } else {
        osFaces.push_back(ptr);
      }
    }
  }

  bool loadFontBundle(std::string const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size <= 0) return false;
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) return false;

    constexpr uint8_t kMagic[8] = {'P','S','O','S','F','N','T','\0'};
    if (bytes.size() < 16 || std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0) {
      return false;
    }

    auto read_u32 = [&](size_t &cursor, uint32_t &out) -> bool {
      if (cursor + 4 > bytes.size()) return false;
      std::memcpy(&out, bytes.data() + cursor, 4);
      cursor += 4;
      return true;
    };

    size_t cursor = 8;
    uint32_t version = 0;
    uint32_t count = 0;
    if (!read_u32(cursor, version) || !read_u32(cursor, count)) return false;
    if (version != 1) return false;

    for (uint32_t i = 0; i < count; ++i) {
      uint32_t nameLen = 0;
      uint32_t dataLen = 0;
      if (!read_u32(cursor, nameLen) || !read_u32(cursor, dataLen)) return false;
      if (cursor + nameLen + dataLen > bytes.size()) return false;
      std::string name;
      if (nameLen > 0) {
        name.assign(reinterpret_cast<const char*>(bytes.data() + cursor), nameLen);
        cursor += nameLen;
      }
      FontBuffer buffer;
      buffer.name = name;
      buffer.data.assign(bytes.begin() + static_cast<long>(cursor),
                         bytes.begin() + static_cast<long>(cursor + dataLen));
      cursor += dataLen;
      bundleBuffers.push_back(std::move(buffer));
      loadFaceMemory(bundleBuffers.back(), /*fromBundle=*/true);
    }
    return true;
  }

  void loadBundledFonts() {
    if (bundlesLoaded) return;
    if (bundleDirs.empty()) {
      return;
    }
    for (auto const& dir : bundleDirs) {
      std::vector<std::string> bundles;
      append_bundle_files(dir, bundles);
      std::sort(bundles.begin(), bundles.end());
      for (auto const& bundle : bundles) {
        loadFontBundle(bundle);
      }
      std::vector<std::string> files;
      append_font_files(dir, files);
      std::sort(files.begin(), files.end());
      for (auto const& file : files) {
        loadFaceFile(file, /*fromBundle=*/true);
      }
    }
    bundlesLoaded = true;
  }

  void loadOsFallbackFonts() {
    if (osFontDirs.empty()) return;
    if (osFilesLoaded) return;
    osFilesLoaded = true;
    for (auto const& dir : osFontDirs) {
      append_font_files(dir, osFontFiles);
    }
    std::sort(osFontFiles.begin(), osFontFiles.end());
  }

  FontFace* loadNextOsFallbackFace() {
    loadOsFallbackFonts();
    while (osFontNext < osFontFiles.size()) {
      std::string path = osFontFiles[osFontNext++];
      size_t before = faces.size();
      loadFaceFile(path, /*fromBundle=*/false);
      if (faces.size() > before) {
        return osFaces.back();
      }
    }
    return nullptr;
  }

  FontFace* selectPrimaryFace(Typography const& typography) {
    loadBundledFonts();
    std::string target = to_lower(typography.family);
    if (target == "bitmap") return nullptr;

    auto pick_best = [&](std::vector<FontFace*> const& faces) -> FontFace* {
      FontFace* best = nullptr;
      int bestScore = std::numeric_limits<int>::max();
      for (auto* face : faces) {
        if (!face) continue;
        if (!target.empty() && target != "default" && to_lower(face->family) != target) continue;
        int score = std::abs(static_cast<int>(face->weight) - static_cast<int>(typography.weight));
        if (face->slant != typography.slant) score += 500;
        if (score < bestScore) {
          bestScore = score;
          best = face;
        }
      }
      return best;
    };

    if (target.empty() || target == "default") {
      if (!bundledFaces.empty()) return bundledFaces.front();
      if (typography.fallback == FontFallbackPolicy::BundleThenOS) {
        loadOsFallbackFonts();
        while (true) {
          if (auto* best = pick_best(osFaces)) return best;
          if (!loadNextOsFallbackFace()) break;
        }
      }
      return nullptr;
    }

    if (auto* best = pick_best(bundledFaces)) return best;
    if (typography.fallback == FontFallbackPolicy::BundleThenOS) {
      loadOsFallbackFonts();
      while (true) {
        if (auto* best = pick_best(osFaces)) return best;
        if (!loadNextOsFallbackFace()) break;
      }
    }
    if (!bundledFaces.empty()) return bundledFaces.front();
    if (!osFaces.empty()) return osFaces.front();
    return nullptr;
  }

  FontFace* resolveFaceForCodepoint(uint32_t codepoint,
                                    FontFace* primary,
                                    FontFallbackPolicy policy) {
    if (primary && face_supports_glyph(primary, codepoint)) return primary;

    uint64_t cacheKey = (static_cast<uint64_t>(primary ? primary->id : 0) << 32) | codepoint;
    auto it = fallbackCache.find(cacheKey);
    if (it != fallbackCache.end()) return it->second;

    for (auto* face : bundledFaces) {
      if (face == primary) continue;
      if (face_supports_glyph(face, codepoint)) {
        fallbackCache[cacheKey] = face;
        return face;
      }
    }

    if (policy == FontFallbackPolicy::BundleOnly) {
      fallbackCache[cacheKey] = primary;
      return primary;
    }

    while (true) {
      for (auto* face : osFaces) {
        if (face_supports_glyph(face, codepoint)) {
          fallbackCache[cacheKey] = face;
          return face;
        }
      }
      if (!loadNextOsFallbackFace()) break;
    }

    fallbackCache[cacheKey] = primary;
    return primary;
  }

  size_t resolveAtlasMax() {
    if (atlasMaxInitialized) return atlasMax;
    atlasMaxInitialized = true;
    if (auto env = std::getenv("PRIMEMANIFEST_FONT_ATLAS_MAX")) {
      char* end = nullptr;
      unsigned long value = std::strtoul(env, &end, 10);
      if (end != env && value > 0) {
        atlasMax = static_cast<size_t>(value);
      }
    }
    return atlasMax;
  }

  std::shared_ptr<GlyphAtlas> allocateAtlasSlot(int width,
                                               int height,
                                               int &outX,
                                               int &outY) {
    if (width <= 0 || height <= 0) return nullptr;
    if (width > AtlasWidth || height > AtlasHeight) return nullptr;

    auto try_allocate = [&](std::shared_ptr<GlyphAtlas> const& atlas) -> bool {
      if (!atlas) return false;
      if (atlas->cursorX + width > atlas->width) {
        atlas->cursorX = 0;
        atlas->cursorY += atlas->rowHeight;
        atlas->rowHeight = 0;
      }
      if (atlas->cursorY + height > atlas->height) return false;
      outX = atlas->cursorX;
      outY = atlas->cursorY;
      atlas->cursorX += width;
      atlas->rowHeight = std::max(atlas->rowHeight, height);
      return true;
    };

    for (auto const& atlas : atlases) {
      if (try_allocate(atlas)) return atlas;
    }

    size_t maxAtlases = resolveAtlasMax();
    if (maxAtlases > 0 && atlases.size() >= maxAtlases) {
      return nullptr;
    }

    auto atlas = std::make_shared<GlyphAtlas>();
    atlas->width = AtlasWidth;
    atlas->height = AtlasHeight;
    atlas->stride = AtlasWidth;
    atlas->cursorX = 0;
    atlas->cursorY = 0;
    atlas->rowHeight = 0;
    atlas->pixels.assign(static_cast<size_t>(AtlasWidth) * AtlasHeight, 0);
    if (!try_allocate(atlas)) return nullptr;
    atlases.push_back(atlas);
    return atlas;
  }

  GlyphBitmap* getGlyphBitmap(FontFace* face,
                              uint32_t glyphId,
                              uint16_t sizePx,
                              uint16_t emboldenStrength) {
    if (!face || !face->face || sizePx == 0) return nullptr;
    GlyphKey key{face->id, sizePx, emboldenStrength, glyphId};
    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) return it->second.get();

    uint16_t effectiveSize = set_face_pixel_size(face->face, sizePx);
    if (effectiveSize == 0) return nullptr;
    FT_Int32 loadFlags = FT_LOAD_DEFAULT | FT_LOAD_COLOR;
    if (FT_Load_Glyph(face->face, glyphId, loadFlags) != 0) return nullptr;
    if (emboldenStrength > 0 && face->face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
      FT_Outline_Embolden(&face->face->glyph->outline, static_cast<FT_Pos>(emboldenStrength));
    }
    if (face->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
      if (FT_Render_Glyph(face->face->glyph, FT_RENDER_MODE_NORMAL) != 0) return nullptr;
    }

    FT_GlyphSlot slot = face->face->glyph;
    FT_Bitmap& bm = slot->bitmap;

    auto bitmap = std::make_unique<GlyphBitmap>();
    bitmap->width = static_cast<int>(bm.width);
    bitmap->height = static_cast<int>(bm.rows);
    bitmap->bearingX = slot->bitmap_left;
    bitmap->bearingY = slot->bitmap_top;
    bitmap->advance = static_cast<int>(slot->advance.x / 64);
    if (bm.buffer && bitmap->width > 0 && bitmap->height > 0) {
      if (bm.pixel_mode == FT_PIXEL_MODE_BGRA) {
        bitmap->format = GlyphBitmapFormat::ColorBGRA;
        bitmap->stride = bitmap->width * 4;
        bitmap->pixels.resize(static_cast<size_t>(bitmap->stride) * bitmap->height);
        int pitch = bm.pitch;
        for (int y = 0; y < bitmap->height; ++y) {
          const uint8_t* srcRow = bm.buffer + (pitch >= 0 ? y * pitch : (bitmap->height - 1 - y) * -pitch);
          uint8_t* dstRow = bitmap->pixels.data() + static_cast<size_t>(y) * bitmap->stride;
          std::memcpy(dstRow, srcRow, static_cast<size_t>(bitmap->stride));
        }
      } else {
        FontBitmapFormat format = FontBitmapFormat::Gray8;
        switch (bm.pixel_mode) {
          case FT_PIXEL_MODE_MONO: format = FontBitmapFormat::Mono1; break;
          default: format = FontBitmapFormat::Gray8; break;
        }

        FontBitmapView view;
        view.buffer = bm.buffer;
        view.width = bitmap->width;
        view.height = bitmap->height;
        view.pitch = bm.pitch;
        view.format = format;

        std::vector<uint8_t> converted;
        int32_t convertedStride = 0;
        if (!ConvertFontBitmapToAlpha(view, converted, convertedStride)) {
          return nullptr;
        }

        bitmap->format = GlyphBitmapFormat::Mask8;
        int atlasX = 0;
        int atlasY = 0;
        auto atlas = allocateAtlasSlot(bitmap->width, bitmap->height, atlasX, atlasY);
        if (atlas) {
          bitmap->atlas = atlas;
          bitmap->atlasX = atlasX;
          bitmap->atlasY = atlasY;
          bitmap->stride = atlas->stride;
          for (int y = 0; y < bitmap->height; ++y) {
            const uint8_t* srcRow = converted.data() + static_cast<size_t>(y) * convertedStride;
            uint8_t* dstRow = atlas->pixels.data() +
                              static_cast<size_t>(atlasY + y) * atlas->stride +
                              static_cast<size_t>(atlasX);
            std::memcpy(dstRow, srcRow, static_cast<size_t>(bitmap->width));
          }
        } else {
          bitmap->stride = bitmap->width;
          bitmap->pixels = std::move(converted);
        }
      }
    }

    GlyphBitmap* out = bitmap.get();
    glyphCache.emplace(key, std::move(bitmap));
    return out;
  }

  std::shared_ptr<TextRun> layoutText(std::string_view text,
                                      Typography const& typography,
                                      float deviceScale,
                                      bool buildGlyphs) {
    loadBundledFonts();
    if (text.empty()) {
      auto run = std::make_shared<TextRun>();
      run->layoutScale = deviceScale;
      return run;
    }

    FontFace* primary = selectPrimaryFace(typography);
    if (!primary) return nullptr;

    float scale = deviceScale > 0.0f ? deviceScale : 1.0f;
    float invScale = 1.0f / scale;
    uint16_t sizePixels = static_cast<uint16_t>(std::max(1.0f, std::round(typography.size * scale)));

    auto codepoints = decode_utf8(text);
    if (codepoints.empty()) return nullptr;

    struct RunSegment {
      FontFace* face;
      size_t startIndex;
      size_t endIndex;
    };

    std::vector<RunSegment> segments;
    segments.reserve(codepoints.size());

    FontFace* currentFace = nullptr;
    size_t segmentStart = 0;
    for (size_t i = 0; i < codepoints.size(); ++i) {
      FontFace* face = resolveFaceForCodepoint(codepoints[i].codepoint, primary, typography.fallback);
      if (!currentFace) {
        currentFace = face;
        segmentStart = i;
        continue;
      }
      if (face != currentFace) {
        segments.push_back(RunSegment{currentFace, segmentStart, i});
        currentFace = face;
        segmentStart = i;
      }
    }
    segments.push_back(RunSegment{currentFace, segmentStart, codepoints.size()});

    auto run = std::make_shared<TextRun>();
    run->layoutScale = scale;
    run->contentHash = 1469598103934665603ull;

    float penX = 0.0f;
    float penY = 0.0f;
    float maxAscender = 0.0f;
    float minDescender = 0.0f;
    float maxRight = 0.0f;
    float maxEmbolden = 0.0f;

    auto features = parse_features(typography.features);

    for (auto const& seg : segments) {
      if (!seg.face || !seg.face->face || seg.startIndex >= seg.endIndex) continue;

      uint16_t effectiveSize = set_face_pixel_size(seg.face->face, sizePixels);
      if (effectiveSize == 0) {
        continue;
      }
      if (seg.face->hbFont) {
        hb_ft_font_set_load_flags(seg.face->hbFont, FT_LOAD_DEFAULT);
        hb_ft_font_changed(seg.face->hbFont);
      }
      uint16_t emboldenStrength = compute_synthetic_bold(seg.face->weight, typography.weight, effectiveSize);
      if (emboldenStrength > 0) {
        float emboldenLogical = static_cast<float>(emboldenStrength) / 64.0f * invScale;
        maxEmbolden = std::max(maxEmbolden, emboldenLogical);
      }

      size_t startByte = codepoints[seg.startIndex].byteOffset;
      size_t endByte = codepoints[seg.endIndex - 1].byteOffset + codepoints[seg.endIndex - 1].byteLength;

      hb_buffer_t* buffer = hb_buffer_create();
      hb_buffer_add_utf8(buffer,
                         text.data() + startByte,
                         static_cast<int>(endByte - startByte),
                         0,
                         static_cast<int>(endByte - startByte));
      if (!typography.locale.empty()) {
        hb_buffer_set_language(buffer, hb_language_from_string(typography.locale.c_str(), -1));
      }
      hb_buffer_guess_segment_properties(buffer);

      hb_shape(seg.face->hbFont,
               buffer,
               features.empty() ? nullptr : features.data(),
               static_cast<unsigned int>(features.size()));

      unsigned int glyphCount = 0;
      hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
      hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);
      auto codepoint_for_cluster = [&](unsigned int cluster) -> std::optional<uint32_t> {
        size_t absolute = startByte + static_cast<size_t>(cluster);
        auto begin = codepoints.begin() + static_cast<long>(seg.startIndex);
        auto end = codepoints.begin() + static_cast<long>(seg.endIndex);
        auto it = std::lower_bound(begin, end, absolute,
                                   [](Utf8Codepoint const& cp, size_t offset) { return cp.byteOffset < offset; });
        if (it != end && it->byteOffset == absolute) return it->codepoint;
        return std::nullopt;
      };

      auto metrics = seg.face->face->size->metrics;
      float ascender = static_cast<float>(metrics.ascender) / 64.0f * invScale;
      float descender = static_cast<float>(metrics.descender) / 64.0f * invScale;
      maxAscender = std::max(maxAscender, ascender);
      minDescender = std::min(minDescender, descender);

      run->contentHash = fnv1a_hash(run->contentHash, static_cast<uint64_t>(emboldenStrength));
      for (unsigned int i = 0; i < glyphCount; ++i) {
        float xOffset = static_cast<float>(positions[i].x_offset) / 64.0f * invScale;
        float yOffset = static_cast<float>(positions[i].y_offset) / 64.0f * invScale;
        float xAdvance = static_cast<float>(positions[i].x_advance) / 64.0f * invScale;
        float yAdvance = static_cast<float>(positions[i].y_advance) / 64.0f * invScale;

        GlyphPlacement placement;
        placement.glyphId = static_cast<int>(infos[i].codepoint);
        placement.x = penX + xOffset;
        placement.y = penY - yOffset;
        if (buildGlyphs) {
          placement.bitmap = getGlyphBitmap(seg.face, infos[i].codepoint, effectiveSize, emboldenStrength);
        }
        run->glyphs.push_back(placement);

        float glyphRight = placement.x + xAdvance;
        if (buildGlyphs && placement.bitmap) {
          glyphRight = std::max(glyphRight,
                                placement.x + (static_cast<float>(placement.bitmap->bearingX +
                                                                  placement.bitmap->width) * invScale));
        } else if (!buildGlyphs) {
          if (FT_Load_Glyph(seg.face->face, infos[i].codepoint, FT_LOAD_DEFAULT) == 0) {
            FT_GlyphSlot slot = seg.face->face->glyph;
            float bearingX = static_cast<float>(slot->metrics.horiBearingX) / 64.0f;
            float glyphWidth = static_cast<float>(slot->metrics.width) / 64.0f;
            float right = placement.x + (bearingX + glyphWidth) * invScale;
            if (emboldenStrength > 0) {
              right += static_cast<float>(emboldenStrength) / 64.0f * invScale;
            }
            glyphRight = std::max(glyphRight, right);
          }
        }

        float letterSpacing = typography.letterSpacing;
        if (std::abs(letterSpacing) <= 1.0f) {
          letterSpacing = typography.size * letterSpacing;
        }

        float wordSpacing = 0.0f;
        if (typography.wordSpacing != 0.0f) {
          bool isSpace = false;
          size_t absolute = startByte + static_cast<size_t>(infos[i].cluster);
          if (absolute < text.size() && text[absolute] == ' ') {
            isSpace = true;
          } else if (auto cp = codepoint_for_cluster(infos[i].cluster); cp && is_word_space(*cp)) {
            isSpace = true;
          }
          if (isSpace) {
            if (std::abs(typography.wordSpacing) <= 1.0f) {
              wordSpacing = xAdvance * typography.wordSpacing;
            } else {
              wordSpacing = typography.wordSpacing;
            }
          }
        }
        penX += xAdvance + letterSpacing + wordSpacing;
        penY += -yAdvance;

        maxRight = std::max(maxRight, glyphRight);

        run->contentHash = fnv1a_hash(run->contentHash, static_cast<uint64_t>(seg.face->id));
        run->contentHash = fnv1a_hash(run->contentHash, static_cast<uint64_t>(placement.glyphId));
        run->contentHash = fnv1a_hash(run->contentHash, static_cast<uint64_t>(std::lround(placement.x * 64.0f)));
        run->contentHash = fnv1a_hash(run->contentHash, static_cast<uint64_t>(std::lround(placement.y * 64.0f)));
      }

      hb_buffer_destroy(buffer);
    }

    run->baseline = maxAscender;
    run->height = maxAscender - minDescender;
    run->width = std::max(penX, maxRight);
    if (maxEmbolden > 0.0f) {
      run->height += maxEmbolden;
      run->width += maxEmbolden;
    }
    if (typography.lineHeight > 0.0f) {
      run->height = typography.lineHeight;
    }

    return run;
  }

  std::pair<int, int> measureText(std::string_view text, Typography const& typography) {
    auto run = layoutText(text, typography, 1.0f, /*buildGlyphs=*/false);
    if (!run) {
      return MeasureUiText(std::string{text}, typography.size);
    }
    int w = static_cast<int>(std::ceil(run->width));
    int h = static_cast<int>(std::ceil(run->height));
    return {w, h};
  }
};
#else
struct FontRegistry::Impl {};
#endif

FontRegistry::FontRegistry()
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
    : impl(std::make_unique<Impl>())
#endif
{
}

FontRegistry::~FontRegistry() = default;

void FontRegistry::addBundleDir(std::string dir) {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  if (!impl || dir.empty()) return;
  std::lock_guard<std::mutex> lock(impl->mutex);
  impl->bundleDirs.push_back(std::move(dir));
#else
  (void)dir;
#endif
}

void FontRegistry::addOsFallbackDir(std::string dir) {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  if (!impl || dir.empty()) return;
  std::lock_guard<std::mutex> lock(impl->mutex);
  impl->osFontDirs.push_back(std::move(dir));
#else
  (void)dir;
#endif
}

void FontRegistry::loadBundledFonts() {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  if (!impl) return;
  std::lock_guard<std::mutex> lock(impl->mutex);
  impl->loadBundledFonts();
#endif
}

void FontRegistry::loadOsFallbackFonts() {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  if (!impl) return;
  std::lock_guard<std::mutex> lock(impl->mutex);
  impl->loadOsFallbackFonts();
#endif
}

bool FontRegistry::hasBundledFaces() const {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  return impl && !impl->bundledFaces.empty();
#else
  return false;
#endif
}

auto FontRegistry::layoutText(std::string_view text,
                              Typography const& typography,
                              float deviceScale,
                              bool buildGlyphs) -> std::shared_ptr<TextRun> {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  if (!impl) return nullptr;
  std::lock_guard<std::mutex> lock(impl->mutex);
  return impl->layoutText(text, typography, deviceScale, buildGlyphs);
#else
  (void)text;
  (void)typography;
  (void)deviceScale;
  (void)buildGlyphs;
  return nullptr;
#endif
}

auto FontRegistry::measureText(std::string_view text,
                               Typography const& typography) -> std::pair<int, int> {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  if (!impl) return {0, 0};
  std::lock_guard<std::mutex> lock(impl->mutex);
  return impl->measureText(text, typography);
#else
  auto [w, h] = MeasureUiText(std::string{text}, typography.size);
  return {w, h};
#endif
}

FontRegistry& GetFontRegistry() {
  static FontRegistry registry;
  return registry;
}

auto LayoutText(std::string_view text,
                Typography const& typography,
                float deviceScale,
                bool buildGlyphs) -> std::shared_ptr<TextRun> {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  return GetFontRegistry().layoutText(text, typography, deviceScale, buildGlyphs);
#else
  (void)text;
  (void)typography;
  (void)deviceScale;
  (void)buildGlyphs;
  return nullptr;
#endif
}

auto MeasureText(std::string_view text,
                 Typography const& typography) -> std::pair<int, int> {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  return GetFontRegistry().measureText(text, typography);
#else
  auto [w, h] = MeasureUiText(std::string{text}, typography.size);
  return {w, h};
#endif
}

} // namespace PrimeManifest
