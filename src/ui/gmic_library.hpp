#pragma once

// The G'MIC filter library as Patchy offers it: the standard library text
// embedded in the application (":/gmic", only in builds with the engine) and
// the catalog of allowlisted filters parsed from it. See docs/gmic.md.

#include "filters/gmic/gmic_catalog.hpp"

#include <string>

namespace patchy::ui {

struct GmicLibrary {
  std::string commands;   // the command file every engine run starts from
  gmic::Catalog catalog;  // allowlisted filters only
};

// Loaded on first use and kept. Empty when the build has no engine.
[[nodiscard]] const GmicLibrary& gmic_library();

// True when the engine is built in and its library loaded with at least one filter.
[[nodiscard]] bool gmic_filters_available();

}  // namespace patchy::ui
