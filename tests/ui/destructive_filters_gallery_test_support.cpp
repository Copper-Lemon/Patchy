#include "test_harness.hpp"
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "filters/filter_registry.hpp"
#include "destructive_filters_gallery_test_support.hpp"
#include "ui_test_support.hpp"

namespace patchy::test::ui {

bool filter_invocations_equal(const patchy::FilterInvocation& lhs,
                              const patchy::FilterInvocation& rhs) {
  return lhs.filter_id == rhs.filter_id &&
         lhs.schema_version == rhs.schema_version &&
         lhs.parameters == rhs.parameters &&
         filter_rgb_equal(lhs.foreground, rhs.foreground) &&
         filter_rgb_equal(lhs.background, rhs.background);
}

}  // namespace patchy::test::ui
