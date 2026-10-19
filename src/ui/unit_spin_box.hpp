#pragma once

#include "ui/measurement_units.hpp"

#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QLocale>
#include <QSpinBox>
#include <QString>
#include <QValidator>

#include <functional>
#include <optional>
#include <vector>

namespace patchy::ui {

// Unit a numeric field natively displays. Deliberately separate from the persisted
// MeasurementUnit (rulers, unit combos, settings tokens) so Degrees never leaks into
// a settings token.
enum class SpinUnit { Pixels, Inches, Centimeters, Millimeters, Points, Percent, Degrees };

// The bridge between the two enums: every MeasurementUnit has a SpinUnit; Degrees has
// no MeasurementUnit (nullopt).
[[nodiscard]] std::optional<MeasurementUnit> measurement_unit_for(SpinUnit unit) noexcept;
[[nodiscard]] SpinUnit spin_unit_for(MeasurementUnit unit) noexcept;

// What the user typed: a number plus an optional unit token. A missing unit means
// "the field's native unit".
struct UnitEntry {
  double value{0.0};
  std::optional<SpinUnit> unit;
};

struct UnitEntryParse {
  // Acceptable: `entry` holds a complete number and (optional) unit token.
  // Intermediate: empty, a bare sign, or a token that is a prefix of a known unit.
  // Invalid: an unknown token or a malformed number.
  QValidator::State state{QValidator::Invalid};
  UnitEntry entry;
  // True when a unit token (complete or partial) is present, so the caller knows
  // the text is not something the stock spin box can handle.
  bool has_unit_token{false};
};

// Parses "200", "200 px", "50%", "2in", "3,5 cm", "45°", ... The spin's own prefix and
// suffix are stripped first so the stock display text parses as a native number.
// Tokens are case-insensitive English abbreviations and names plus the localized
// suffixes from measurement_units.hpp. The number is read with `locale` (group
// separators rejected) and falls back to the C locale so "1.5" works everywhere.
[[nodiscard]] UnitEntryParse parse_unit_entry(QString text, const QLocale& locale,
                                              const QString& prefix = QString(),
                                              const QString& suffix = QString());

struct UnitConversionContext {
  double ppi{300.0};                      // document resolution; sanitized on use
  double percent_reference_pixels{0.0};   // what 100% means for THIS field; 0 = no basis
};

// Converts an entry into the field's native unit. nullopt when the typed unit is
// incompatible (a length in a degree field, a percent without a basis, ...).
[[nodiscard]] std::optional<double> convert_unit_entry(const UnitEntry& entry, SpinUnit native,
                                                       const UnitConversionContext& context);

// Localized display suffix for a native unit, exactly what the measurement_units.hpp
// helpers produce (" px", " in", " cm", " mm", " pt", "%", the degree sign).
[[nodiscard]] QString spin_unit_suffix(SpinUnit unit);

// Drops trailing zeros after `locale`'s decimal point, and then a bare point, from a
// formatted number: "12.000" -> "12", "1.50" -> "1.5". Text without a decimal point
// is returned unchanged.
[[nodiscard]] QString trim_trailing_decimal_zeros(QString text, const QLocale& locale);

// A unitless QDoubleSpinBox (aspect-ratio parts) that always displays trimmed
// numbers, as UnitSpinBox::set_trim_trailing_zeros does: "1", "1.5", never "1.00".
// decimals() still sets the precision a typed value keeps.
class TrimmedDoubleSpinBox : public QDoubleSpinBox {
  Q_OBJECT

 public:
  explicit TrimmedDoubleSpinBox(QWidget* parent = nullptr);

 protected:
  QString textFromValue(double value) const override;
};

// A QDoubleSpinBox that accepts a unit token after the number and converts it into
// the field's native unit (Photoshop's numeric-field behavior). Plain numbers behave
// exactly like the stock spin box; the display always shows the native unit.
class UnitSpinBox : public QDoubleSpinBox {
  Q_OBJECT

 public:
  using ContextProvider = std::function<UnitConversionContext()>;

  explicit UnitSpinBox(SpinUnit native, QWidget* parent = nullptr);

  [[nodiscard]] SpinUnit native_unit() const noexcept { return native_; }
  void set_context_provider(ContextProvider provider);
  [[nodiscard]] UnitConversionContext conversion_context() const;
  // Re-applies the localized suffix after a language change.
  void refresh_suffix();

  // Presentation unit. value() stays in the native unit; the suffix and the number
  // shown convert through the context, and a plain typed number is read in this
  // unit. A percent display without a basis falls back to the native text.
  // Leaving the native unit also widens decimals() to the display unit's
  // (measurement_unit_decimals) and sets singleStep() to one display unit converted
  // through the context (measurement_unit_single_step), so arrow keys and a
  // scrubby-label drag move a sensible amount in inches or cm; returning to the
  // native unit restores the decimals and step the field was built with. Set the
  // native decimals and step before switching units, never while one is shown.
  void set_display_unit(SpinUnit unit);
  [[nodiscard]] SpinUnit display_unit() const noexcept { return display_; }
  // Recomputes the display-unit step for the current context (the PPI changed:
  // another document became active, or Image Size edited the resolution).
  void refresh_display_metrics();
  // Photoshop's X/Y/W/H fields: a typed unit token becomes the display unit, and a
  // right-click offers the unit list.
  void set_display_unit_switchable(bool enabled);
  [[nodiscard]] bool display_unit_switchable() const noexcept { return switchable_; }
  // What the right-click menu does: shows `unit` and emits display_unit_picked, which
  // MainWindow routes to the ruler unit preference (Photoshop: a field's unit menu
  // changes Units & Rulers for every field and the rulers). A typed unit token goes
  // through set_display_unit alone and stays this field's choice. No-op unless
  // switchable.
  void pick_display_unit(SpinUnit unit);
  // Shows "12 pt" instead of "12.000 pt": trailing zeros after the decimal point
  // (and a bare point) drop from the display while decimals() keeps the precision.
  void set_trim_trailing_zeros(bool enabled);
  [[nodiscard]] bool trims_trailing_zeros() const noexcept { return trim_trailing_zeros_; }

 signals:
  // Every display-unit change, whatever caused it (the linked transform W/H pair
  // follows through this).
  void display_unit_changed(patchy::ui::SpinUnit unit);
  // The user picked `unit` from the field's unit menu (pick_display_unit).
  void display_unit_picked(patchy::ui::SpinUnit unit);

 protected:
  void changeEvent(QEvent* event) override;
  QValidator::State validate(QString& input, int& pos) const override;
  double valueFromText(const QString& text) const override;
  QString textFromValue(double value) const override;
  void fixup(QString& input) const override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  [[nodiscard]] UnitEntry effective_entry(UnitEntry entry) const;

  SpinUnit native_;
  SpinUnit display_;
  bool switchable_{false};
  bool trim_trailing_zeros_{false};
  // The presentation the field was built with, captured when a display unit first
  // replaces the native one and restored when the native unit comes back.
  std::optional<int> native_decimals_;
  std::optional<double> native_single_step_;
  ContextProvider context_provider_;
  // Set by valueFromText (const) while a typed token is being committed; applied
  // once the edit finishes so the field re-renders in the new unit.
  mutable std::optional<SpinUnit> pending_display_unit_;
};

// Integer twin for the many whole-pixel fields; converted values round to the
// nearest integer.
class UnitIntSpinBox : public QSpinBox {
  Q_OBJECT

 public:
  using ContextProvider = std::function<UnitConversionContext()>;

  explicit UnitIntSpinBox(SpinUnit native, QWidget* parent = nullptr);

  [[nodiscard]] SpinUnit native_unit() const noexcept { return native_; }
  void set_context_provider(ContextProvider provider);
  [[nodiscard]] UnitConversionContext conversion_context() const;
  void refresh_suffix();

 protected:
  void changeEvent(QEvent* event) override;
  QValidator::State validate(QString& input, int& pos) const override;
  int valueFromText(const QString& text) const override;
  void fixup(QString& input) const override;

 private:
  SpinUnit native_;
  ContextProvider context_provider_;
};

// What a pixel-native dimension field needs to present itself in the user's unit
// (docs/resolution-units.md): the ruler unit (`view/rulerUnits`), the document PPI,
// and the document extent as the Percent basis per axis (0 = no basis).
struct DocumentFieldUnits {
  MeasurementUnit display_unit{MeasurementUnit::Pixels};
  double ppi{300.0};
  double document_width{0.0};
  double document_height{0.0};
  // Where a field's unit-menu pick goes (MainWindow fills it with the ruler unit
  // preference setter, so the pick lands on the rulers and every live field, as in
  // Photoshop). Empty: the pick stays inside the dialog.
  std::function<void(MeasurementUnit)> on_unit_picked;
};

// The conversion context for one axis of `units`.
[[nodiscard]] UnitConversionContext document_field_context(const DocumentFieldUnits& units, bool horizontal);

// Makes `spin` switchable and shows it in `unit` (a no-op switch still refreshes the
// step for the current context). The caller supplies the context provider.
void set_field_display_unit(UnitSpinBox* spin, MeasurementUnit unit);

// For fields in a modal dialog: a fixed context snapshot for the axis plus
// set_field_display_unit, and the field's unit-menu pick forwarded to
// `units.on_unit_picked`. Fields whose live document can change (options bar,
// panels) keep their own provider and call set_field_display_unit directly.
void apply_document_field_units(UnitSpinBox* spin, const DocumentFieldUnits& units, bool horizontal);

// A dialog's dimension fields read as one control: a unit picked on any of them
// shows on all of them (the app-wide preference change covers only MainWindow's
// enrolled live fields, not a modal dialog's).
void link_field_unit_picks(const std::vector<UnitSpinBox*>& fields);

}  // namespace patchy::ui
