#include "ui/layer_style_dialog_controls.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/theme_qss.hpp"
#include <QAbstractButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace patchy::ui::layer_style_detail {
void update_color_preview_label(QWidget* widget, int red, int green, int blue) {
  set_themed_style(*widget, QStringLiteral("%1 { background: rgb(%2, %3, %4); border: 1px solid @swatch_border; }")
                           .arg(widget->metaObject()->className())
                           .arg(red)
                           .arg(green)
                           .arg(blue));
}

RgbColor RgbColorRowWidgets::value() const {
    return RgbColor{static_cast<std::uint8_t>(red->value()), static_cast<std::uint8_t>(green->value()),
                    static_cast<std::uint8_t>(blue->value())};
}

void RgbColorRowWidgets::set_value(RgbColor color) const {
    red->setValue(color.red);
    green->setValue(color.green);
    blue->setValue(color.blue);
}

void RgbColorRowWidgets::update_preview() const {
    update_color_preview_label(preview, red->value(), green->value(), blue->value());
}

QString slider_object_name(QString spin_object_name) {
    if (spin_object_name.endsWith(QStringLiteral("Spin"))) {
      spin_object_name.chop(4);
    }
    return spin_object_name + QStringLiteral("Slider");
}
QSpinBox* add_slider_spin_row(QFormLayout* form, QWidget* parent, const QString& label,
                                                   const QString& spin_object_name, int minimum, int maximum,
                                                   int value, const QString& suffix, int spin_width,
                                                   int slider_maximum) {
    return add_dialog_slider_spin_row(form, parent, label, slider_object_name(spin_object_name),
                                      spin_object_name, minimum, maximum, value, suffix, spin_width,
                                      /*row_spacing=*/8, /*step_buttons=*/true, slider_maximum);
}
  // Effect sizes and distances: most picks are small, so the slider gives the
  // low end most of its track (SliderCurve::FineLowEnd).
QSpinBox* add_size_slider_spin_row(QFormLayout* form, QWidget* parent, const QString& label,
                                                        const QString& spin_object_name, int minimum,
                                                        int maximum, int value,
                                                        int slider_maximum) {
    return add_dialog_slider_spin_row(form, parent, label, slider_object_name(spin_object_name),
                                      spin_object_name, minimum, maximum, value, {}, 72,
                                      /*row_spacing=*/8, /*step_buttons=*/true, slider_maximum,
                                      SliderCurve::FineLowEnd);
}
QSpinBox* add_color_slider_row(QVBoxLayout* layout, QWidget* parent, const QString& label,
                                                    const QString& spin_object_name, std::uint8_t value) {
    auto* row = new QWidget(parent);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(8);
    auto* channel_label = new QLabel(label, row);
    channel_label->setFixedWidth(18);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setObjectName(slider_object_name(spin_object_name));
    slider->setRange(0, 255);
    slider->setValue(value);
    auto* spin = new QSpinBox(row);
    spin->setObjectName(spin_object_name);
    spin->setRange(0, 255);
    spin->setValue(value);
    configure_dialog_spinbox(spin, 54);
    row_layout->addWidget(channel_label);
    row_layout->addWidget(slider, 1);
    row_layout->addWidget(spin);
    QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    layout->addWidget(row);
    return spin;
}
  // The "Color RGB" form row: R/G/B slider rows plus a clickable preview swatch
  // button below them. Widgets follow the shared naming scheme
  // (<prefix>RedSpin/<prefix>GreenSpin/<prefix>BlueSpin/<prefix>ColorPreview).
RgbColorRowWidgets make_color_rows(QFormLayout* form, QWidget* group,
                                                 const QString& object_prefix, RgbColor initial) {
    RgbColorRowWidgets rows;
    auto* color_row = new QWidget(group);
    auto* color_layout = new QVBoxLayout(color_row);
    color_layout->setContentsMargins(0, 0, 0, 0);
    color_layout->setSpacing(4);
    rows.row_container = color_row;
    rows.red = add_color_slider_row(color_layout, color_row, QObject::tr("R"),
                                    object_prefix + QStringLiteral("RedSpin"), initial.red);
    rows.green = add_color_slider_row(color_layout, color_row, QObject::tr("G"),
                                      object_prefix + QStringLiteral("GreenSpin"), initial.green);
    rows.blue = add_color_slider_row(color_layout, color_row, QObject::tr("B"),
                                     object_prefix + QStringLiteral("BlueSpin"), initial.blue);
    auto* preview_row = new QWidget(color_row);
    auto* preview_layout = new QHBoxLayout(preview_row);
    preview_layout->setContentsMargins(26, 0, 0, 0);
    preview_layout->setSpacing(8);
    auto* preview = new QPushButton(group);
    preview->setObjectName(object_prefix + QStringLiteral("ColorPreview"));
    preview->setFixedSize(28, 22);
    preview->setToolTip(QObject::tr("Choose Color..."));
    preview_layout->addWidget(preview);
    preview_layout->addStretch(1);
    color_layout->addWidget(preview_row);
    rows.preview = preview;
    rows.picker = preview;
    rows.update_preview();
    form->addRow(QObject::tr("Color RGB"), color_row);
    return rows;
}

}
