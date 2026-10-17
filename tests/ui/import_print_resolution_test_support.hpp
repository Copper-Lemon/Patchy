#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

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

// Repeatedly runs `step` on a short timer while open_document_path blocks in the import
// dialog's exec() loop; `step` returns true when its work is done. Same shape as the raw
// develop dialog's driver in camera_raw_heif_tests.cpp.
void drive_modal_dialog(const std::shared_ptr<std::function<bool()>>& step, int attempts = 2400);

// A whole PDF from its numbered objects (object 1 is the catalog), with the cross-reference
// offsets computed, never hand-counted.
QByteArray pdf_from_objects(const std::vector<std::string>& objects);

#endif  // PATCHY_HAVE_QT_PDF

}  // namespace patchy::test::ui::import_print_resolution_support
