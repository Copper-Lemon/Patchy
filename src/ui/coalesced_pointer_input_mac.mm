// macOS half of coalesced_pointer_input.hpp. AppKit merges queued mouse and
// tablet drag events into one while the main thread is busy, the same loss
// Windows and X11 show (GitHub issue 165); a paint stroke turns that off for
// its length. The setting is process-wide and read per incoming event.

#include "ui/coalesced_pointer_input.hpp"

#import <AppKit/AppKit.h>

namespace patchy::ui {

bool set_mac_mouse_coalescing_enabled(bool enabled) {
  const bool previous = [NSEvent isMouseCoalescingEnabled] == YES;
  [NSEvent setMouseCoalescingEnabled:enabled ? YES : NO];
  return previous;
}

}  // namespace patchy::ui
