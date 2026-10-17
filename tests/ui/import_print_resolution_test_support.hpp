#pragma once

class QByteArray;
class QImage;
class QString;
namespace patchy { class Document; class Layer; struct RgbColor; }

namespace patchy::test::ui::import_print_resolution_support {

#if defined(PATCHY_HAVE_QT_PDF)

QByteArray read_file_bytes(const QString& path);

// PDFium renders onto a transparent page; Patchy's composite over white is the
// reference, so both sides are flattened onto white before comparing.
QImage over_white(const QImage& image);

double mean_rgb_delta_over_white(const QImage& a, const QImage& b);

patchy::Layer solid_rect_shape_layer(patchy::Document& document, const char* name, int left, int top, int right,
                                     int bottom, patchy::RgbColor fill, double stroke_width);

#endif  // PATCHY_HAVE_QT_PDF

}  // namespace patchy::test::ui::import_print_resolution_support
