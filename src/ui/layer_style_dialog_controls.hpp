#pragma once
// Private building blocks for the Layer Style dialog, never a UI umbrella.
#include "core/layer.hpp"
#include <QString>
#include <cstdint>
#include <limits>
class QAbstractButton;
class QFormLayout;
class QSpinBox;
class QVBoxLayout;
class QWidget;
namespace patchy::ui::layer_style_detail {
struct RgbColorRowWidgets {
  QSpinBox* red{};
  QSpinBox* green{};
  QSpinBox* blue{};
  QWidget* preview{};
  QAbstractButton* picker{};
  QWidget* row_container{};
  [[nodiscard]] RgbColor value() const;
  void set_value(RgbColor color) const;
  void update_preview() const;
};

void update_color_preview_label(QWidget* widget, int red, int green, int blue);
QString slider_object_name(QString spin_object_name);
QSpinBox* add_slider_spin_row(QFormLayout* form, QWidget* parent, const QString& label,
                                                   const QString& spin_object_name, int minimum, int maximum,
                                                   int value, const QString& suffix = {}, int spin_width = 72,
                                                   int slider_maximum = std::numeric_limits<int>::max());
QSpinBox* add_size_slider_spin_row(QFormLayout* form, QWidget* parent, const QString& label,
                                                        const QString& spin_object_name, int minimum,
                                                        int maximum, int value,
                                                        int slider_maximum = std::numeric_limits<int>::max());
QSpinBox* add_color_slider_row(QVBoxLayout* layout, QWidget* parent, const QString& label,
                                                    const QString& spin_object_name, std::uint8_t value);
RgbColorRowWidgets make_color_rows(QFormLayout* form, QWidget* group,
                                                 const QString& object_prefix, RgbColor initial);

}  // namespace patchy::ui::layer_style_detail
