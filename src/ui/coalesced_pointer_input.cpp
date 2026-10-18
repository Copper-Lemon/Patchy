#include "ui/coalesced_pointer_input.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace patchy::ui {

namespace {

// True when `time` was recorded before `reference` (system ticks wrap).
bool tick_before(std::uint32_t time, std::uint32_t reference) noexcept {
  return static_cast<std::int32_t>(time - reference) < 0;
}

#ifdef Q_OS_WIN

// The native screen position Qt built `event` from. Qt maps the message's
// client coordinates to the window's logical position by the scale factor
// alone, so the window-relative position times the window's device pixel
// ratio rounds back to the exact client pixel.
std::optional<PointerHistoryPoint> native_point_for_event(const QWidget& widget, const QMouseEvent& event,
                                                          qreal* device_pixel_ratio) {
  if (event.source() != Qt::MouseEventNotSynthesized ||
      QGuiApplication::platformName().compare(QStringLiteral("windows"), Qt::CaseInsensitive) != 0) {
    return std::nullopt;
  }
  const auto* top_level = widget.window();
  const auto* window = top_level != nullptr ? top_level->windowHandle() : nullptr;
  if (window == nullptr) {
    return std::nullopt;
  }
  const auto ratio = window->devicePixelRatio();
  if (!(ratio > 0.0)) {
    return std::nullopt;
  }
  const auto local = event.scenePosition();
  POINT point{static_cast<LONG>(std::lround(local.x() * ratio)), static_cast<LONG>(std::lround(local.y() * ratio))};
  if (!ClientToScreen(reinterpret_cast<HWND>(window->winId()), &point)) {
    return std::nullopt;
  }
  *device_pixel_ratio = ratio;
  return PointerHistoryPoint{static_cast<std::int32_t>(point.x), static_cast<std::int32_t>(point.y),
                             static_cast<std::uint32_t>(event.timestamp())};
}

// GetMouseMovePointsEx, newest first, starting with `current` itself.
std::vector<PointerHistoryPoint> native_mouse_history(PointerHistoryPoint current) {
  MOUSEMOVEPOINT input{};
  // The history stores display points as 16-bit values; negative coordinates
  // (monitors left of or above the primary) come back wrapped.
  input.x = current.x & 0xFFFF;
  input.y = current.y & 0xFFFF;
  input.time = current.time;
  std::array<MOUSEMOVEPOINT, 64> output{};
  const auto count = GetMouseMovePointsEx(sizeof(MOUSEMOVEPOINT), &input, output.data(),
                                          static_cast<int>(output.size()), GMMP_USE_DISPLAY_POINTS);
  if (count <= 0) {
    return {};
  }
  std::vector<PointerHistoryPoint> history;
  history.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    auto x = output[static_cast<std::size_t>(i)].x;
    auto y = output[static_cast<std::size_t>(i)].y;
    if (x > 32767) {
      x -= 65536;
    }
    if (y > 32767) {
      y -= 65536;
    }
    history.push_back(PointerHistoryPoint{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                                          static_cast<std::uint32_t>(output[static_cast<std::size_t>(i)].time)});
  }
  // The supplied point leads the result; anything else means the history no
  // longer matches this message (it is cleared on some desktop switches).
  if (history.front().x != current.x || history.front().y != current.y) {
    return {};
  }
  return history;
}

#endif

}  // namespace

std::vector<PointerHistoryPoint> pointer_history_since(std::span<const PointerHistoryPoint> newest_first,
                                                       std::optional<PointerHistoryPoint> previous) {
  std::vector<PointerHistoryPoint> recovered;
  if (newest_first.size() < 2 || !previous.has_value()) {
    return recovered;
  }
  for (std::size_t i = 1; i < newest_first.size(); ++i) {
    const auto& point = newest_first[i];
    if (point == *previous || tick_before(point.time, previous->time)) {
      break;
    }
    // A repeat of the newer neighbour adds nothing to the path.
    const auto& newer = newest_first[i - 1];
    if (point.x == newer.x && point.y == newer.y) {
      continue;
    }
    recovered.push_back(point);
  }
  std::reverse(recovered.begin(), recovered.end());
  if (!recovered.empty() && recovered.front().x == previous->x && recovered.front().y == previous->y) {
    recovered.erase(recovered.begin());
  }
  return recovered;
}

void begin_coalesced_mouse_tracking(const QWidget& widget, const QMouseEvent& event, CoalescedMouseState& state) {
  state.previous.reset();
#ifdef Q_OS_WIN
  qreal ratio = 1.0;
  state.previous = native_point_for_event(widget, event, &ratio);
#else
  Q_UNUSED(widget);
  Q_UNUSED(event);
#endif
}

std::vector<QPointF> coalesced_mouse_positions(const QWidget& widget, const QMouseEvent& event,
                                               CoalescedMouseState& state) {
  std::vector<QPointF> positions;
#ifdef Q_OS_WIN
  qreal ratio = 1.0;
  const auto current = native_point_for_event(widget, event, &ratio);
  if (!current.has_value()) {
    state.previous.reset();
    return positions;
  }
  const auto previous = state.previous;
  state.previous = current;
  if (!previous.has_value()) {
    return positions;
  }
  const auto history = native_mouse_history(*current);
  const auto recovered = pointer_history_since(history, previous);
  positions.reserve(recovered.size());
  const auto origin = event.position();
  for (const auto& point : recovered) {
    positions.push_back(origin + QPointF(point.x - current->x, point.y - current->y) / ratio);
  }
#else
  Q_UNUSED(widget);
  Q_UNUSED(event);
  Q_UNUSED(state);
#endif
  return positions;
}

UncompressedPointerInputScope::~UncompressedPointerInputScope() {
  set_engaged(false);
}

void UncompressedPointerInputScope::set_engaged(bool engaged) {
  if (engaged == engaged_) {
    return;
  }
  if (engaged) {
    saved_compression_ = QCoreApplication::testAttribute(Qt::AA_CompressHighFrequencyEvents);
    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, false);
  } else {
    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, saved_compression_);
  }
  engaged_ = engaged;
}

}  // namespace patchy::ui
