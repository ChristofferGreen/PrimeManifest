#pragma once

#include "PrimeManifest/text/TextLayout.hpp"
#include "PrimeManifest/text/Typography.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace PrimeManifest {

class FontRegistry {
public:
  FontRegistry();
  ~FontRegistry();

  FontRegistry(FontRegistry const&) = delete;
  FontRegistry& operator=(FontRegistry const&) = delete;

  void addBundleDir(std::string dir);
  void addOsFallbackDir(std::string dir);
  void loadBundledFonts();
  void loadOsFallbackFonts();
  bool hasBundledFaces() const;

  auto layoutText(std::string_view text,
                  Typography const& typography,
                  float deviceScale,
                  bool buildGlyphs = true) -> std::shared_ptr<TextRun>;

  auto measureText(std::string_view text,
                   Typography const& typography) -> std::pair<int, int>;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

auto GetFontRegistry() -> FontRegistry&;

auto LayoutText(std::string_view text,
                Typography const& typography,
                float deviceScale,
                bool buildGlyphs = true) -> std::shared_ptr<TextRun>;

auto MeasureText(std::string_view text,
                 Typography const& typography) -> std::pair<int, int>;

} // namespace PrimeManifest
