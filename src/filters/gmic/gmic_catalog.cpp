#include "filters/gmic/gmic_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>

namespace patchy::gmic {

namespace {

[[nodiscard]] std::string trimmed(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return {};
  }
  const auto last = text.find_last_not_of(" \t\r\n");
  return std::string(text.substr(first, last - first + 1));
}

// Names carry Pango-style markup ("<b>Colors</b>", "&amp;").
[[nodiscard]] std::string plain_name(std::string_view marked_up) {
  std::string out;
  bool in_tag = false;
  for (const char ch : marked_up) {
    if (ch == '<') {
      in_tag = true;
    } else if (ch == '>') {
      in_tag = false;
    } else if (!in_tag) {
      out.push_back(ch);
    }
  }
  static const std::pair<std::string_view, std::string_view> kEntities[] = {
      {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}};
  for (const auto& [entity, replacement] : kEntities) {
    for (auto at = out.find(entity); at != std::string::npos; at = out.find(entity, at + replacement.size())) {
      out.replace(at, entity.size(), replacement);
    }
  }
  return trimmed(out);
}

// '#@gui' lines with their backslash continuations, which continue on plain
// '#' comment lines. Per-language '#@gui_xx' lines are not read.
[[nodiscard]] std::vector<std::string> declaration_lines(std::string_view commands) {
  std::vector<std::string> lines;
  bool continuing = false;
  std::size_t at = 0;
  while (at <= commands.size()) {
    auto end = commands.find('\n', at);
    if (end == std::string_view::npos) {
      end = commands.size();
    }
    std::string_view line = commands.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (continuing) {
      if (!line.empty() && line.front() == '#') {
        line.remove_prefix(1);
      }
      continuing = !line.empty() && line.back() == '\\';
      if (continuing) {
        line.remove_suffix(1);
      }
      lines.back().append(line);
      continue;
    }
    constexpr std::string_view kPrefix = "#@gui ";
    if (line.substr(0, kPrefix.size()) != kPrefix) {
      continue;
    }
    line.remove_prefix(kPrefix.size());
    continuing = !line.empty() && line.back() == '\\';
    if (continuing) {
      line.remove_suffix(1);
    }
    lines.emplace_back(line);
  }
  return lines;
}

[[nodiscard]] std::optional<ParameterType> parameter_type(std::string_view name) {
  static const std::pair<std::string_view, ParameterType> kTypes[] = {
      {"bool", ParameterType::Bool},     {"button", ParameterType::Button},       {"choice", ParameterType::Choice},
      {"color", ParameterType::Color},   {"point", ParameterType::Point},         {"file", ParameterType::File},
      {"filein", ParameterType::File},   {"fileout", ParameterType::File},        {"file_in", ParameterType::File},
      {"file_out", ParameterType::File}, {"float", ParameterType::Float},         {"folder", ParameterType::Folder},
      {"int", ParameterType::Int},       {"link", ParameterType::Link},           {"note", ParameterType::Note},
      {"text", ParameterType::Text},     {"separator", ParameterType::Separator}, {"value", ParameterType::Value},
  };
  for (const auto& [known, type] : kTypes) {
    if (name == known) {
      return type;
    }
  }
  return std::nullopt;
}

// Arguments split at commas outside double quotes; a quoted argument loses its quotes.
[[nodiscard]] std::vector<std::string> split_arguments(std::string_view text) {
  std::vector<std::string> arguments;
  std::string current;
  bool quoted = false;
  bool any = false;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char ch = text[index];
    if (ch == '\\' && quoted && index + 1 < text.size() && text[index + 1] == '"') {
      current.push_back('"');
      ++index;
    } else if (ch == '"') {
      quoted = !quoted;
      any = true;
    } else if (ch == ',' && !quoted) {
      arguments.push_back(any ? current : trimmed(current));
      current.clear();
      any = false;
    } else if (quoted || any) {
      // After a closing quote only separators follow; stray spaces are dropped.
      if (quoted) {
        current.push_back(ch);
      }
    } else {
      current.push_back(ch);
    }
  }
  if (any || !trimmed(current).empty() || !arguments.empty()) {
    arguments.push_back(any ? current : trimmed(current));
  }
  return arguments;
}

[[nodiscard]] bool is_number(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  char* end = nullptr;
  std::strtod(text.c_str(), &end);
  return end != nullptr && *end == '\0';
}

[[nodiscard]] double number_or(const std::vector<std::string>& arguments, std::size_t index, double fallback) {
  return index < arguments.size() && is_number(arguments[index]) ? std::strtod(arguments[index].c_str(), nullptr)
                                                                 : fallback;
}

[[nodiscard]] std::string number_text(const std::vector<std::string>& arguments, std::size_t index,
                                      std::string_view fallback) {
  return index < arguments.size() && is_number(arguments[index]) ? arguments[index] : std::string(fallback);
}

[[nodiscard]] int hex_digit(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return lower >= 'a' && lower <= 'f' ? lower - 'a' + 10 : -1;
}

// Fills the typed fields and the default value from the raw arguments.
void interpret(Parameter& parameter, std::string_view raw) {
  const auto& arguments = parameter.arguments;
  switch (parameter.type) {
    case ParameterType::Bool: {
      const std::string value = arguments.empty() ? std::string() : arguments.front();
      parameter.default_value = value == "1" || value == "true" ? "1" : "0";
      break;
    }
    case ParameterType::Button:
      parameter.default_value = "0";
      break;
    case ParameterType::Choice: {
      // An integer first argument followed by labels is the default index.
      std::size_t first_label = 0;
      long default_index = 0;
      if (arguments.size() > 1 && is_number(arguments.front())) {
        default_index = std::strtol(arguments.front().c_str(), nullptr, 10);
        first_label = 1;
      }
      parameter.choices.assign(arguments.begin() + static_cast<std::ptrdiff_t>(first_label), arguments.end());
      if (default_index < 0 || default_index >= static_cast<long>(parameter.choices.size())) {
        default_index = 0;
      }
      parameter.default_value = std::to_string(default_index);
      break;
    }
    case ParameterType::Color: {
      std::vector<std::string> channels;
      if (arguments.size() == 1 && !arguments.front().empty() && arguments.front().front() == '#') {
        const std::string& hex = arguments.front();
        for (std::size_t index = 1; index + 1 < hex.size(); index += 2) {
          const int high = hex_digit(hex[index]);
          const int low = hex_digit(hex[index + 1]);
          if (high < 0 || low < 0) {
            break;
          }
          channels.push_back(std::to_string(high * 16 + low));
        }
      } else {
        for (const auto& argument : arguments) {
          if (is_number(argument)) {
            channels.push_back(argument);
          }
        }
      }
      // One value is a gray level; missing green and blue repeat the red.
      while (channels.size() < 3) {
        channels.push_back(channels.empty() ? std::string("0") : channels.front());
      }
      if (channels.size() > 4) {
        channels.resize(4);
      }
      parameter.has_alpha = channels.size() == 4;
      for (std::size_t index = 0; index < channels.size(); ++index) {
        parameter.default_value += (index == 0 ? "" : ",") + channels[index];
      }
      break;
    }
    case ParameterType::Point:
      parameter.default_value = number_text(arguments, 0, "50") + "," + number_text(arguments, 1, "50");
      break;
    case ParameterType::File:
    case ParameterType::Folder:
      parameter.text = arguments.empty() ? std::string() : arguments.front();
      parameter.default_value = quoted_text_value(parameter.text);
      break;
    case ParameterType::Float:
    case ParameterType::Int:
      parameter.default_value = number_text(arguments, 0, "0");
      parameter.minimum = number_or(arguments, 1, 0.0);
      parameter.maximum = number_or(arguments, 2, parameter.type == ParameterType::Float ? 1.0 : 100.0);
      break;
    case ParameterType::Link: {
      // link(_alignment,_label,URL): the URL is last, the label before it.
      if (!arguments.empty()) {
        parameter.url = arguments.back();
        parameter.text = arguments.size() > 1 && !is_number(arguments[arguments.size() - 2])
                             ? arguments[arguments.size() - 2]
                             : parameter.url;
      }
      break;
    }
    case ParameterType::Note: {
      // The whole content is the label; commas inside it are text.
      std::string label = trimmed(raw);
      if (label.size() >= 2 && label.front() == '"' && label.back() == '"') {
        label = label.substr(1, label.size() - 2);
      }
      parameter.text = label;
      break;
    }
    case ParameterType::Text: {
      std::size_t first_text = 0;
      if (arguments.size() > 1 && (arguments.front() == "0" || arguments.front() == "1")) {
        parameter.multiline = arguments.front() == "1";
        first_text = 1;
      }
      for (std::size_t index = first_text; index < arguments.size(); ++index) {
        parameter.text += (index == first_text ? "" : ",") + arguments[index];
      }
      parameter.default_value = quoted_text_value(parameter.text);
      break;
    }
    case ParameterType::Separator:
      break;
    case ParameterType::Value:
      // One argument whatever it holds: "value(50,50)" reaches the filter as a single
      // quoted argument that the filter itself splits (its "${1-11}" then yields twelve
      // values). Unquoted, every later argument would shift by one.
      parameter.text = trimmed(raw);
      parameter.default_value = quoted_text_value(parameter.text);
      break;
  }
}

// One filter's joined parameter text: "name = type(args)_1+, name = type[args] name = ...".
// Typedefs are separated by commas or by nothing but white space.
void parse_parameters(std::string_view text, Filter& filter, std::vector<std::string>& diagnostics) {
  std::size_t at = 0;
  const auto fail = [&](std::string_view reason) {
    diagnostics.push_back(filter.name + ": " + std::string(reason) + " near \"" +
                          std::string(text.substr(at, std::min<std::size_t>(60, text.size() - at))) + "\"");
  };
  while (at < text.size()) {
    while (at < text.size() && (std::isspace(static_cast<unsigned char>(text[at])) != 0 || text[at] == ',')) {
      ++at;
    }
    if (at >= text.size()) {
      return;
    }
    const auto equals = text.find('=', at);
    if (equals == std::string_view::npos) {
      fail("no '=' after a parameter name");
      return;
    }
    Parameter parameter;
    parameter.name = trimmed(text.substr(at, equals - at));
    std::size_t type_start = text.find_first_not_of(" \t", equals + 1);
    if (type_start == std::string_view::npos) {
      fail("no type after '='");
      return;
    }
    for (; type_start < text.size() && (text[type_start] == '_' || text[type_start] == '~'); ++type_start) {
      if (text[type_start] == '_') {
        parameter.updates_preview = false;
      } else {
        parameter.randomizable = true;
      }
    }
    std::size_t open = type_start;
    while (open < text.size() && (std::isalpha(static_cast<unsigned char>(text[open])) != 0 || text[open] == '_')) {
      ++open;
    }
    const auto type = parameter_type(text.substr(type_start, open - type_start));
    // "note (...)": white space may separate the type from its arguments.
    while (open < text.size() && (text[open] == ' ' || text[open] == '\t')) {
      ++open;
    }
    if (!type.has_value() || open >= text.size()) {
      fail("unknown parameter type");
      return;
    }
    parameter.type = *type;
    const char opener = text[open];
    const char closer = opener == '(' ? ')' : opener == '[' ? ']' : opener == '{' ? '}' : '\0';
    const auto close = closer == '\0' ? std::string_view::npos : text.find(closer, open + 1);
    if (close == std::string_view::npos) {
      fail("unterminated parameter arguments");
      return;
    }
    const std::string_view raw = text.substr(open + 1, close - open - 1);
    parameter.arguments = split_arguments(raw);
    interpret(parameter, raw);
    at = close + 1;
    // Visibility state "_0", "_1", "_2" and its propagation mark.
    if (at + 1 < text.size() && text[at] == '_' && text[at + 1] >= '0' && text[at + 1] <= '2') {
      parameter.visibility = text[at + 1] == '0'   ? ParameterVisibility::Hidden
                             : text[at + 1] == '1' ? ParameterVisibility::Disabled
                                                   : ParameterVisibility::Visible;
      at += 2;
      if (at < text.size() && (text[at] == '+' || text[at] == '-' || text[at] == '*')) {
        parameter.visibility_propagation = text[at];
        ++at;
      }
    }
    filter.parameters.push_back(std::move(parameter));
  }
}

// "Name : command, preview(factor)+ : mode". False for the '_none_' pages.
[[nodiscard]] bool parse_filter_line(std::string_view line, Filter& filter) {
  const auto colon = line.find(':');
  filter.name = plain_name(line.substr(0, colon));
  std::string rest = trimmed(line.substr(colon + 1));
  const auto mode_colon = rest.rfind(':');
  if (mode_colon != std::string::npos) {
    const std::string mode = trimmed(std::string_view(rest).substr(mode_colon + 1));
    if (mode.size() == 1 && std::string_view("x.*+-vi").find(mode.front()) != std::string_view::npos) {
      filter.input_mode = mode.front();
      rest = trimmed(std::string_view(rest).substr(0, mode_colon));
    }
  }
  const auto comma = rest.find(',');
  filter.command = trimmed(std::string_view(rest).substr(0, comma));
  if (filter.command.empty() || filter.command == "_none_") {
    return false;
  }
  if (comma == std::string::npos) {
    return true;
  }
  std::string preview = trimmed(std::string_view(rest).substr(comma + 1));
  const auto open = preview.find('(');
  if (open != std::string::npos) {
    const auto close = preview.find(')', open);
    const std::string factor = close == std::string::npos ? std::string() : preview.substr(open + 1, close - open - 1);
    if (is_number(factor)) {
      filter.preview_factor = std::strtod(factor.c_str(), nullptr);
    }
    if (close != std::string::npos) {
      const std::string marks = preview.substr(close + 1);
      filter.preview_accurate_at_any_zoom = marks.find('+') != std::string::npos;
      filter.preview_needs_whole_image = marks.find('*') != std::string::npos;
    }
    preview = trimmed(std::string_view(preview).substr(0, open));
  }
  if (preview != "_none_" && preview != filter.command) {
    filter.preview_command = preview;
  }
  return true;
}

}  // namespace

bool Parameter::carries_value() const noexcept {
  return type != ParameterType::Note && type != ParameterType::Link && type != ParameterType::Separator;
}

std::size_t Filter::value_count() const noexcept {
  return static_cast<std::size_t>(
      std::count_if(parameters.begin(), parameters.end(), [](const Parameter& p) { return p.carries_value(); }));
}

const Filter* Catalog::find(std::string_view id) const noexcept {
  const auto found = std::find_if(filters.begin(), filters.end(), [&](const Filter& f) { return f.id == id; });
  return found == filters.end() ? nullptr : &*found;
}

Catalog parse_catalog(std::string_view commands) {
  Catalog catalog;
  std::vector<std::string> folders;
  std::map<std::string, int> command_uses;
  Filter current;
  std::string parameters;
  bool have_filter = false;
  const auto finish = [&] {
    if (have_filter) {
      parse_parameters(parameters, current, catalog.diagnostics);
      catalog.filters.push_back(std::move(current));
    }
    current = Filter{};
    parameters.clear();
    have_filter = false;
  };
  for (const auto& raw : declaration_lines(commands)) {
    const std::string line = trimmed(raw);
    if (line.empty()) {
      continue;
    }
    if (line.front() == ':') {
      // A typedef's arguments may run over several ':' lines.
      parameters += trimmed(std::string_view(line).substr(1));
      parameters.push_back(' ');
      continue;
    }
    finish();
    if (line.find(':') == std::string::npos) {
      // A folder line: each leading '_' leaves one folder, then the name (if any) opens one.
      std::size_t leave = 0;
      while (leave < line.size() && line[leave] == '_') {
        ++leave;
      }
      for (std::size_t index = 0; index < leave && !folders.empty(); ++index) {
        folders.pop_back();
      }
      const std::string name = plain_name(std::string_view(line).substr(leave));
      if (!name.empty()) {
        folders.push_back(name);
      }
      continue;
    }
    current.folder_path = folders;
    if (!parse_filter_line(line, current)) {
      current = Filter{};
      // Its parameter lines are dropped with it.
      parameters.clear();
      continue;
    }
    const int uses = ++command_uses[current.command];
    current.id = uses == 1 ? current.command : current.command + "#" + std::to_string(uses);
    have_filter = true;
  }
  finish();
  return catalog;
}

std::vector<std::string> parse_allowlist(std::string_view text) {
  std::vector<std::string> ids;
  std::size_t at = 0;
  while (at < text.size()) {
    auto end = text.find('\n', at);
    if (end == std::string_view::npos) {
      end = text.size();
    }
    std::string_view line = text.substr(at, end - at);
    at = end + 1;
    // A comment starts the line or follows white space; an id's own "#2"
    // suffix follows the command directly.
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string_view::npos || line[first] == '#') {
      continue;
    }
    line.remove_prefix(first);
    ids.emplace_back(line.substr(0, line.find_first_of(" \t\r")));
  }
  return ids;
}

void retain_allowed(Catalog& catalog, const std::vector<std::string>& allowed_ids) {
  std::erase_if(catalog.filters, [&](const Filter& filter) {
    return std::find(allowed_ids.begin(), allowed_ids.end(), filter.id) == allowed_ids.end();
  });
}

std::vector<std::string> default_values(const Filter& filter) {
  std::vector<std::string> values;
  for (const auto& parameter : filter.parameters) {
    if (parameter.carries_value()) {
      values.push_back(parameter.default_value);
    }
  }
  return values;
}

void release_button_values(const Filter& filter, std::vector<std::string>& values) {
  std::size_t index = 0;
  for (const auto& parameter : filter.parameters) {
    if (!parameter.carries_value()) {
      continue;
    }
    if (parameter.type == ParameterType::Button && index < values.size()) {
      values[index] = "0";
    }
    ++index;
  }
}

std::string command_line(const Filter& filter, const std::vector<std::string>& values, bool preview) {
  std::string line = preview && !filter.preview_command.empty() ? filter.preview_command : filter.command;
  const auto defaults = default_values(filter);
  for (std::size_t index = 0; index < defaults.size(); ++index) {
    line += index == 0 ? ' ' : ',';
    line += index < values.size() ? values[index] : defaults[index];
  }
  return line;
}

OutputLayerInfo parse_output_layer_name(std::string_view image_name) {
  OutputLayerInfo info;
  // Each field is "key(...)"; fields are comma separated and the value runs to the
  // parenthesis that closes it, so a name may itself hold commas and parentheses.
  const auto field = [&](std::string_view key) -> std::optional<std::string> {
    std::size_t at = 0;
    while ((at = image_name.find(key, at)) != std::string_view::npos) {
      const bool starts_field = at == 0 || image_name[at - 1] == ',';
      const std::size_t open = at + key.size();
      if (starts_field && open < image_name.size() && image_name[open] == '(') {
        int depth = 0;
        for (std::size_t index = open; index < image_name.size(); ++index) {
          if (image_name[index] == '(') {
            ++depth;
          } else if (image_name[index] == ')' && --depth == 0) {
            return std::string(image_name.substr(open + 1, index - open - 1));
          }
        }
        return std::string(image_name.substr(open + 1));
      }
      at = open;
    }
    return std::nullopt;
  };
  const auto mode = field("mode");
  const auto opacity = field("opacity");
  const auto position = field("pos");
  const auto name = field("name");
  if (!mode.has_value() && !opacity.has_value() && !position.has_value() && !name.has_value()) {
    info.name = trimmed(image_name);
    return info;
  }
  if (name.has_value()) {
    info.name = trimmed(*name);
  }
  if (mode.has_value()) {
    for (const char ch : trimmed(*mode)) {
      info.blend_mode.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
  }
  if (opacity.has_value() && is_number(trimmed(*opacity))) {
    info.opacity = std::clamp(std::strtod(trimmed(*opacity).c_str(), nullptr), 0.0, 100.0);
  }
  if (position.has_value()) {
    const auto comma = position->find(',');
    if (comma != std::string::npos) {
      const std::string x = trimmed(std::string_view(*position).substr(0, comma));
      const std::string y = trimmed(std::string_view(*position).substr(comma + 1));
      if (is_number(x) && is_number(y)) {
        info.position = std::pair<int, int>{static_cast<int>(std::strtod(x.c_str(), nullptr)),
                                            static_cast<int>(std::strtod(y.c_str(), nullptr))};
      }
    }
  }
  return info;
}

std::string quoted_text_value(std::string_view text) {
  std::string out = "\"";
  for (const char ch : text) {
    if (ch == '"' || ch == '\\') {
      out.push_back('\\');
    }
    out.push_back(ch);
  }
  out.push_back('"');
  return out;
}

}  // namespace patchy::gmic
