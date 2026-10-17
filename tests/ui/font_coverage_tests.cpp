// Every character a shipped catalog uses must exist in a font the web build bundles
// (docs/fonts.md, docs/localization.md).
//
// The browser exposes no system fonts, so on wasm the bundled tree under
// third_party/fonts-web is the whole font inventory: a character no bundled face
// covers renders as a box. Desktop builds fall back to the operating system's fonts
// and are not affected, which is why this reads the source tree rather than the
// running font database. Before September 2026 the only bundled CJK face was
// Noto Sans JP, and 279 of Simplified Chinese's 1132 characters had no glyph.
//
// A new translation that introduces an uncovered character fails here. The fix is a
// wider bundled font, not a reworded translation.

#include "test_harness.hpp"
#include "ui_test_groups.hpp"
#include "ui/localization.hpp"
#include "ui/ui_font.hpp"
#include <QDir>
#include <QFile>
#include <QFont>
#include <QRawFont>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

QStringList bundled_web_font_files() {
  const QDir root(QStringLiteral(PATCHY_SOURCE_DIR "/third_party/fonts-web"));
  QStringList files;
  for (const auto& family : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
    const QDir dir(root.filePath(family));
    for (const auto& file :
         dir.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files, QDir::Name)) {
      files.push_back(dir.filePath(file));
    }
  }
  return files;
}

// Characters that appear in a catalog's translated text. The English sources are left
// out on purpose: the point is what a translation adds.
QList<char32_t> catalog_characters(const QString& language) {
  const auto path = QStringLiteral(PATCHY_SOURCE_DIR "/translations/patchy_%1.ts").arg(language);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "cannot open %s\n", qPrintable(path));
    CHECK(false);
  }
  QXmlStreamReader xml(&file);
  QSet<char32_t> characters;
  bool in_translation = false;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement() && xml.name() == u"translation") {
      in_translation = true;
    } else if (xml.isEndElement() && xml.name() == u"translation") {
      in_translation = false;
    } else if (in_translation && xml.isCharacters()) {
      for (const auto rune : xml.text().toString().toUcs4()) {
        if (rune != U'\n' && rune != U'\r' && rune != U'\t') {
          characters.insert(rune);
        }
      }
    }
  }
  CHECK(!xml.hasError());
  auto list = characters.values();
  std::sort(list.begin(), list.end());
  return list;
}

void ui_bundled_web_fonts_cover_every_catalog_character() {
  std::vector<QRawFont> fonts;
  for (const auto& file : bundled_web_font_files()) {
    QRawFont font(file, 16.0);
    if (!font.isValid()) {
      std::fprintf(stderr, "unreadable bundled font %s\n", qPrintable(file));
      CHECK(false);
    }
    fonts.push_back(std::move(font));
  }
  CHECK(fonts.size() > 20);

  QStringList failures;
  for (const auto& language : patchy::ui::LocalizationManager::instance().languages()) {
    if (language.code == patchy::ui::LocalizationManager::source_language()) {
      continue;  // English is the source text; the Latin faces cover it
    }
    QStringList missing;
    const auto characters = catalog_characters(language.code);
    CHECK(!characters.isEmpty());
    for (const auto rune : characters) {
      const bool covered = std::any_of(fonts.begin(), fonts.end(), [rune](const QRawFont& font) {
        return font.supportsCharacter(static_cast<uint>(rune));
      });
      if (!covered) {
        missing.push_back(QStringLiteral("U+%1 %2")
                              .arg(static_cast<uint>(rune), 4, 16, QLatin1Char('0'))
                              .arg(QString::fromUcs4(&rune, 1)));
      }
    }
    if (!missing.isEmpty()) {
      failures.push_back(QStringLiteral("%1: %2 of %3 characters have no glyph in any bundled web font: %4")
                             .arg(language.code)
                             .arg(missing.size())
                             .arg(characters.size())
                             .arg(missing.mid(0, 40).join(QStringLiteral(", "))));
    }
  }
  for (const auto& failure : failures) {
    std::fprintf(stderr, "%s\n", qPrintable(failure));
  }
  CHECK(failures.isEmpty());
}

void ui_wasm_cjk_fallback_order_follows_the_language() {
  using patchy::ui::wasm_cjk_fallback_families;
  const auto jp = QStringLiteral("Noto Sans JP");
  const auto sc = QStringLiteral("Noto Sans SC");
  const auto tc = QStringLiteral("Noto Sans TC");
  const auto ko = QStringLiteral("NanumGothic");
  // The three families share most Han codepoints and Qt takes the first that has the
  // glyph, so the language decides whose shapes win.
  CHECK(wasm_cjk_fallback_families(QStringLiteral("zh_CN")) == QStringList({sc, tc, jp, ko}));
  CHECK(wasm_cjk_fallback_families(QStringLiteral("zh_TW")) == QStringList({tc, sc, jp, ko}));
  CHECK(wasm_cjk_fallback_families(QStringLiteral("ja")) == QStringList({jp, sc, tc, ko}));
  CHECK(wasm_cjk_fallback_families(QStringLiteral("en")) == QStringList({jp, sc, tc, ko}));
  CHECK(wasm_cjk_fallback_families(QString()) == QStringList({jp, sc, tc, ko}));
  CHECK(wasm_cjk_fallback_families(QStringLiteral("ko")) == QStringList({ko, jp, sc, tc}));
  CHECK(wasm_cjk_fallback_families(QStringLiteral("ko-KR")) == QStringList({ko, jp, sc, tc}));
  // Every shipped language resolves to a list naming each bundled CJK family once.
  for (const auto& language : patchy::ui::LocalizationManager::instance().languages()) {
    const auto families = wasm_cjk_fallback_families(language.code);
    CHECK(families.size() == 4);
    CHECK(families.contains(jp) && families.contains(sc) && families.contains(tc) && families.contains(ko));
  }
}

void ui_bundled_web_fonts_cover_modern_korean_in_both_weights() {
  for (const auto* weight : {"Regular", "Bold"}) {
    const auto path = QStringLiteral(PATCHY_SOURCE_DIR "/third_party/fonts-web/nanum_gothic/NanumGothic-%1.ttf")
                          .arg(QString::fromLatin1(weight));
    const QRawFont font(path, 16.0);
    CHECK(font.isValid());
    CHECK(font.familyName() == QStringLiteral("NanumGothic"));
    CHECK(font.weight() == (QString::fromLatin1(weight) == QStringLiteral("Bold") ? QFont::Bold : QFont::Normal));
    for (uint codepoint = 0xAC00; codepoint <= 0xD7A3; ++codepoint) {
      CHECK(font.supportsCharacter(codepoint));
    }
    const auto glyphs = font.glyphIndexesForString(QStringLiteral("한국어 파일 편집 레이어 저장"));
    CHECK(!glyphs.isEmpty());
    CHECK(std::all_of(glyphs.cbegin(), glyphs.cend(), [](quint32 glyph) { return glyph != 0; }));
  }
}

}  // namespace

std::vector<patchy::test::TestCase> font_coverage_tests() {
  return {
      {"ui_bundled_web_fonts_cover_every_catalog_character", ui_bundled_web_fonts_cover_every_catalog_character},
      {"ui_bundled_web_fonts_cover_modern_korean_in_both_weights", ui_bundled_web_fonts_cover_modern_korean_in_both_weights},
      {"ui_wasm_cjk_fallback_order_follows_the_language", ui_wasm_cjk_fallback_order_follows_the_language},
  };
}
