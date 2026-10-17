#include "ui/build_info.hpp"

#include "patchy_build_stamp.hpp"

namespace patchy::ui {

QString build_timestamp_text() {
  // PATCHY_BUILD_STAMP is "yyyy-MM-dd HH:mm" in the build machine's local
  // wall clock, written into a generated header that the patchy_build_stamp
  // rule refreshes after source/build inputs change or packaging requests it.
  // Unchanged builds reuse the binary and its date. All release builds run in
  // JST, so the label is hardcoded rather than guessed at runtime: a user's
  // machine cannot recover the build zone from a naive timestamp.
  return QStringLiteral(PATCHY_BUILD_STAMP " JST");
}

}  // namespace patchy::ui
