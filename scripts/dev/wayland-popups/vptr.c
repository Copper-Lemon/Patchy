// Persistent virtual pointer for a headless wlroots compositor (docs/wayland.md), built by
// run.sh against wlr-virtual-pointer-unstable-v1. Reads commands from stdin:
//   move X Y   (absolute layout pixels in a W x H extent given on the command line)
//   click      (left button press + release)
//   press      (left button down; stays down until release, for drags)
//   release
//   sleep MS
// The device lives until stdin closes, so clients keep their wl_pointer.
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "vptr-client.h"

static struct wl_seat *seat;
static struct zwlr_virtual_pointer_manager_v1 *manager;

static void global(void *data, struct wl_registry *reg, uint32_t name, const char *iface,
                   uint32_t version) {
  (void)data;
  if (strcmp(iface, wl_seat_interface.name) == 0) {
    seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
  } else if (strcmp(iface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
    manager = wl_registry_bind(reg, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
  }
  (void)version;
}
static void global_remove(void *data, struct wl_registry *reg, uint32_t name) {
  (void)data; (void)reg; (void)name;
}
static const struct wl_registry_listener reg_listener = {global, global_remove};

static uint32_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int main(int argc, char **argv) {
  const uint32_t w = argc > 2 ? atoi(argv[1]) : 1400, h = argc > 2 ? atoi(argv[2]) : 900;
  struct wl_display *d = wl_display_connect(NULL);
  if (!d) { fprintf(stderr, "no display\n"); return 1; }
  struct wl_registry *reg = wl_display_get_registry(d);
  wl_registry_add_listener(reg, &reg_listener, NULL);
  wl_display_roundtrip(d);
  if (!manager) { fprintf(stderr, "no virtual pointer manager\n"); return 1; }
  struct zwlr_virtual_pointer_v1 *p =
      zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, seat);
  wl_display_roundtrip(d);
  char line[128];
  while (fgets(line, sizeof line, stdin)) {
    unsigned x, y, ms;
    if (sscanf(line, "move %u %u", &x, &y) == 2) {
      zwlr_virtual_pointer_v1_motion_absolute(p, now_ms(), x, y, w, h);
      zwlr_virtual_pointer_v1_frame(p);
    } else if (strncmp(line, "click", 5) == 0) {
      zwlr_virtual_pointer_v1_button(p, now_ms(), BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
      zwlr_virtual_pointer_v1_frame(p);
      wl_display_roundtrip(d);
      usleep(50000);
      zwlr_virtual_pointer_v1_button(p, now_ms(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
      zwlr_virtual_pointer_v1_frame(p);
    } else if (strncmp(line, "press", 5) == 0) {
      zwlr_virtual_pointer_v1_button(p, now_ms(), BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
      zwlr_virtual_pointer_v1_frame(p);
    } else if (strncmp(line, "release", 7) == 0) {
      zwlr_virtual_pointer_v1_button(p, now_ms(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
      zwlr_virtual_pointer_v1_frame(p);
    } else if (sscanf(line, "sleep %u", &ms) == 1) {
      wl_display_roundtrip(d);
      usleep(ms * 1000);
    }
    wl_display_roundtrip(d);
  }
  zwlr_virtual_pointer_v1_destroy(p);
  wl_display_roundtrip(d);
  wl_display_disconnect(d);
  return 0;
}
