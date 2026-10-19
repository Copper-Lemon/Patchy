#pragma once

#include <QLabel>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace patchy {
class Document;
}

namespace patchy::ui {

// Memory the document's own buffers hold: every layer's pixels and raster mask plus
// the document channels, summed from byte sizes (O(layer count), never a flatten).
[[nodiscard]] std::uint64_t document_layer_bytes(const Document& document);

// The status-bar document info field (GitHub issues 202 and 206; see
// docs/view-navigation.md): one line with the active document's dimensions,
// resolution, color mode and bit depth, in that order. When the bar is narrow the
// trailing parts drop off whole; the dimensions always stay. The tooltip carries the
// whole line plus the color profile and memory use.
class StatusDocumentInfoLabel final : public QLabel {
  Q_OBJECT

public:
  // Inputs for the color part that live outside the Document: CMYK Proof is a
  // session state, shown through the working CMYK profile (docs/cmyk.md).
  struct ColorContext {
    bool cmyk_proof{false};
    QString cmyk_profile_name;
  };

  struct Info {
    QStringList parts;  // most important first
    QString tooltip;
  };

  explicit StatusDocumentInfoLabel(QWidget* parent = nullptr);

  [[nodiscard]] static Info describe(const Document& document, const ColorContext& color);
  [[nodiscard]] static QString separator();

  // Shows `info` or, with no parts, hides the field. A no-op when nothing changed.
  void set_info(const Info& info);
  // The most text width the field may take; trailing parts that do not fit drop.
  void set_width_budget(int pixels);
  [[nodiscard]] const QStringList& parts() const noexcept { return parts_; }

private:
  void update_shown_text();

  QStringList parts_;
  int width_budget_{0};
};

}  // namespace patchy::ui
