#pragma once

// The controls of one G'MIC filter, generated from its declared parameters
// (gmic_catalog.hpp): sliders, choices, check boxes, color swatches, text, point
// coordinates and buttons, with the filter's notes, links and separators laid
// out between them. Parameter names and note text are G'MIC's own and stay in
// English (docs/gmic.md). The engine is not involved.

#include "filters/gmic/gmic_catalog.hpp"

#include <QPointF>
#include <QString>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QDoubleSpinBox;
class QFormLayout;

namespace patchy::ui {

class GmicParameterPanel : public QWidget {
  Q_OBJECT

public:
  explicit GmicParameterPanel(QWidget* parent = nullptr);

  // Replaces the controls with `filter`'s, at their default values, or at
  // `values` (one per value-carrying parameter, G'MIC argument form) where given.
  void set_filter(const gmic::Filter& filter, const std::vector<std::string>& values = {});

  // One value per value-carrying parameter, in declaration order, as the filter's
  // command line takes them.
  [[nodiscard]] std::vector<std::string> values() const;
  void set_values(const std::vector<std::string>& values);
  void reset_to_defaults();

  // Called after a control changes. The argument is false for a parameter the filter
  // declared as not refreshing the preview ('_typedef').
  void set_changed_callback(std::function<void(bool updates_preview)> callback);

  // What a filter returned as its status after a run: "{v1}{v2}..." with one entry per
  // value-carrying parameter, each optionally followed by a visibility state (_0 hidden,
  // _1 disabled, _2 visible). Sets those values and states without reporting a change.
  // False when the status is not of that form for this filter.
  bool apply_status(const std::string& status);

  // Puts every pressed button back to 0 without reporting a change. A button is a one-shot
  // flag: the run that read it as 1 has used it.
  void release_buttons();

  // The filter's visible point parameters, in percent of the image the filter works on,
  // in declaration order; and moving one (which reports a change like any edit). The
  // callback runs whenever the list or a position changes, however it changed.
  [[nodiscard]] std::vector<QPointF> points() const;
  void set_point(int index, QPointF percent);
  void set_points_changed_callback(std::function<void()> callback);

private:
  struct Binding {
    std::string default_value;
    std::function<std::string()> read;
    std::function<void(const std::string&)> write;
    QWidget* field{nullptr};  // the row's field; null for a hidden value
    bool button{false};
  };

  struct PointControl {
    QDoubleSpinBox* x{nullptr};
    QDoubleSpinBox* y{nullptr};
  };

  void clear();
  void changed(bool updates_preview);
  void points_changed();

  QFormLayout* form_{nullptr};
  std::vector<Binding> bindings_;
  std::function<void(bool)> changed_callback_;
  std::vector<PointControl> point_controls_;
  std::function<void()> points_changed_callback_;
  bool updating_{false};
};

// G'MIC note text (Pango-style markup) as Qt rich text.
[[nodiscard]] QString gmic_note_rich_text(const std::string& note);

}  // namespace patchy::ui
