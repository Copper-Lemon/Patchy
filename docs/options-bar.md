# Options bar layout

How the Options bar keeps one height and fits narrow windows (GitHub issue 91). Control conventions (row height, scrub labels, `configure_toolbar_spinbox`, unit fields) stay in [ui-conventions.md](ui-conventions.md).

## One height per window width

- The bar's height depends on the window width, never on the active tool. A height change on a tool switch moves the tool palette, the canvas and the docks.
- `MainWindow::refresh_options_bar_rows` hands `FlowLayout` (`src/ui/options_flow_layout.hpp`) every tool's row: the widgets `refresh_options_bar` would show for that tool under the current vector mode, live shape layer and crop style. It rebuilds the rows only when that key changes. The text session's apply/cancel pair counts as part of the Text row, so opening an editor never grows the bar.
- `OptionsFlowContainer` reports the tallest row at its width, or the live row if that is taller (transform and warp sessions are not tool rows).
- Hidden widgets are measured the way `QWidgetItem` would measure them shown, after `ensurePolished`. A widget added to the flow outside `register_option_action` is invisible to the measure and can still grow the bar.

## Duplicate sliders drop first

- An inline slider that repeats the spin box beside it (the spin box already has the popup slider and the scrub label) carries `kOptionsBarCollapsibleProperty`. Today: Brush Size, Opacity and Soft, Gradient Opacity, Quick Select Size, Fill Opacity and Soft.
- At a width where any row holding one would wrap with it, all of them hide, for every tool at once, so the bar looks the same from tool to tool. A resize across that width calls back into `refresh_options_bar`.
- Mark only true duplicates. Never mark a control that is the only way to reach a setting.

## Text bar

- At 1366 px every tool row must fit on one line: `ui_options_bar_keeps_one_height_across_tools` checks it and prints each row's natural width. A new control on a long row (Text, Brush, Clone) needs that budget.
- Text Size shows Photoshop's precision: at most two decimals, trailing zeros dropped ("12 pt", "14.45 pt"; `UnitSpinBox::set_trim_trailing_zeros`). Sizes are whole pixels underneath (`text_points_to_pixels`), so two decimals round-trip every pixel size below 7200 ppi. The automatic size compares against the value the field holds after rounding (`text_size_auto_points_`).
- A field whose displayed max is narrower than its realistic values sizes itself from `kToolbarSpinboxWidthSampleProperty` (Text Size: 888.88 pt).
- The font family combo's popup sizes itself; the style combo's list opens wider than the closed control.
- Paragraph direction is in the Paragraph panel with a mirror of the Vertical toggle, as in Photoshop; the bar keeps the toggle. The color swatch has no "Color:" label, and the align buttons are ragged-line glyphs (`text_alignment_icon`, columns for vertical type) with no "Align:" label. Option labels pad 5 px on the left only, controls sit 4 px apart, and options-bar checkboxes size to their text with a 58 px floor; the old widths were chip-era.
