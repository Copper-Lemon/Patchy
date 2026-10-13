#pragma once

// The G'MIC filter catalog: the '#@gui' declarations of a G'MIC command file
// parsed into filters and typed parameters, and the argument text a filter is
// called with. Qt-free and independent of the engine. See docs/gmic.md.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace patchy::gmic {

enum class ParameterType {
  Bool,
  Button,
  Choice,
  Color,
  Point,
  File,
  Float,
  Folder,
  Int,
  Link,
  Note,
  Text,
  Separator,
  Value,
};

enum class ParameterVisibility { Hidden, Disabled, Visible };

struct Parameter {
  std::string name;
  ParameterType type{ParameterType::Note};
  // The typedef's arguments, split at top-level commas with quotes removed.
  std::vector<std::string> arguments;
  // The value this parameter contributes to the command line by default, in
  // G'MIC argument form ("1.5", "2", "255,0,0,255", "\"text\""). Empty for the
  // types that carry no value.
  std::string default_value;
  double minimum{0.0};  // Float and Int
  double maximum{0.0};
  std::vector<std::string> choices;  // Choice
  std::string text;                  // Note and Link label, Text default, Value content
  std::string url;                   // Link
  bool multiline{false};             // Text
  bool has_alpha{false};             // Color
  bool updates_preview{true};        // false for '_typedef'
  bool randomizable{false};          // '~typedef'
  ParameterVisibility visibility{ParameterVisibility::Visible};
  char visibility_propagation{'\0'};  // '+', '-', '*' or none

  // Note, Link and Separator are layout only.
  [[nodiscard]] bool carries_value() const noexcept;
};

struct Filter {
  // The apply command, made unique with a "#2" suffix when two filters share one.
  // Persisted by scripts and presets once the feature ships: never renumber.
  std::string id;
  std::string name;
  std::vector<std::string> folder_path;
  std::string command;
  std::string preview_command;  // empty: the apply command previews
  // The declared preview zoom: 0 is 1:1, 1 the whole image, 2 half size; negative when none was declared.
  double preview_factor{-1.0};
  bool preview_accurate_at_any_zoom{false};  // '+' after the factor
  bool preview_needs_whole_image{false};     // '*' after the factor
  // x none, . active, * all, + active and below, - active and above, v visible, i invisible.
  char input_mode{'.'};
  std::vector<Parameter> parameters;

  [[nodiscard]] std::size_t value_count() const noexcept;
};

struct Catalog {
  std::vector<Filter> filters;
  // Declarations that could not be read, one line each.
  std::vector<std::string> diagnostics;

  [[nodiscard]] const Filter* find(std::string_view id) const noexcept;
};

// Reads every '#@gui' declaration of a command file. Entries declared with the
// '_none_' command (information pages) are skipped.
[[nodiscard]] Catalog parse_catalog(std::string_view commands);

// The ids one per line that may be offered; '#' starts a comment. See
// patchy_filters_allowlist.txt.
[[nodiscard]] std::vector<std::string> parse_allowlist(std::string_view text);

// Drops every filter whose id is not listed.
void retain_allowed(Catalog& catalog, const std::vector<std::string>& allowed_ids);

// One default value per value-carrying parameter, in declaration order.
[[nodiscard]] std::vector<std::string> default_values(const Filter& filter);

// Sets every button parameter's entry of `values` (one per value-carrying parameter) back
// to "0". A button is a one-shot flag: it reads 1 for the single run after its press.
void release_button_values(const Filter& filter, std::vector<std::string>& values);

// "command v1,v2,..." for the apply or the preview command. `values` holds one
// entry per value-carrying parameter; missing entries use the defaults.
[[nodiscard]] std::string command_line(const Filter& filter, const std::vector<std::string>& values, bool preview);

// What a filter says about an output image through its name:
// "mode(grainmerge),opacity(50),pos(30,50),name(Glow)". A name without any of
// those fields is a plain layer name.
struct OutputLayerInfo {
  std::string name;
  std::string blend_mode;                         // G'MIC's blend name, lower case; empty when not stated
  std::optional<double> opacity;                  // 0..100
  std::optional<std::pair<int, int>> position;    // x, y in the image the filter worked on
};
[[nodiscard]] OutputLayerInfo parse_output_layer_name(std::string_view image_name);

// A text value as G'MIC expects it on a command line: quoted, with quotes and
// backslashes escaped.
[[nodiscard]] std::string quoted_text_value(std::string_view text);

}  // namespace patchy::gmic
