#pragma once
// Text document construction and run formatting shared by editing and rendering.
// No window or canvas ownership crosses this interface.
#include "core/vector_shape.hpp"
#include <QColor>
#include <QFont>
#include <QString>
#include <QTextDocument>
#include <QTextCharFormat>
#include <cstdint>
#include <memory>
#include <optional>
namespace patchy::ui {
inline constexpr int kMinimumTextBoxDocumentSize = 16;
struct TextToolSettings {
  QString text;
  QString html;
  QString family;
  int size{36};
  bool bold{false};
  bool italic{false};
  int anti_alias{3};
  bool boxed{false};
  int box_width{0};
  int box_height{0};
  // Photoshop leading-model layout (kLayerMetadataTextLayoutMode == "photoshop"): baselines
  // advance by each entered line's max leading instead of Qt's natural spacing.
  bool photoshop_layout{false};
  // Vertical type (kLayerMetadataTextOrientation == "vertical"): upright glyphs stacked in
  // columns that advance right to left. See ui/text_layout.hpp.
  bool vertical{false};
  std::optional<VectorPath> area{std::nullopt};
};


// The document a text layer is rasterized from, plus the font/width the layout was built against.
// Built by build_text_render_document -- the single construction shared by the rasterizer and the
// caret/selection layout so glyphs and caret geometry can never diverge.
struct TextRenderDocument {
  std::unique_ptr<QTextDocument> document;
  QFont font;
  int text_width{0};
};


QString paragraph_alignment_name(Qt::Alignment alignment);
Qt::Alignment paragraph_alignment_from_name(const QString& name);
void set_text_display_family(QTextCharFormat& format, const QString& family);
QString text_display_family_from_format(const QTextCharFormat& format, const QString& fallback);
QTextCharFormat text_format_with_smoothing(QTextCharFormat format, int anti_alias);
void apply_text_smoothing_to_document(QTextDocument& document, int anti_alias);
void set_stretch_for_advance_ratio(QFont& font, double ratio, int naive_value);
void scale_document_font_sizes(QTextDocument& document, double scale);
std::unique_ptr<QTextDocument> copy_text_document_formats(const QTextDocument& source);
QString rich_text_runs_from_document(const QTextDocument& document, const TextToolSettings& fallback,
                                     QColor fallback_color);
QString paragraph_runs_from_document(const QTextDocument& document);
void apply_paragraph_runs_to_document(QTextDocument& document, const QString& paragraph_runs, double scale = 1.0);
void apply_patchy_text_runs_to_document(QTextDocument& document, const QString& runs_text, const QFont& fallback_font,
                                        QColor fallback_color, double scale, int anti_alias);
QString document_html_from_text_runs(const QString& text, const QString& rich_text_runs,
                                     const TextToolSettings& fallback, QColor fallback_color);
void substitute_missing_document_font_families(QTextDocument& document);
TextRenderDocument build_text_render_document(const TextToolSettings& settings, QColor color,
                                              std::int32_t max_width, const QString& paragraph_runs,
                                              const QString& rich_text_runs, double metric_scale,
                                              double layout_scale = 1.0, double box_scale = 1.0);
QString first_text_run_style_name(const QString& rich_text_runs);

}  // namespace patchy::ui
