#include "core/stroke_patch.hpp"

#include "core/document.hpp"
#include "core/document_memory.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace patchy {

namespace {

struct TargetView {
  const PixelBuffer* buffer{nullptr};
  // Document position of the buffer's (0, 0).
  std::int32_t origin_x{0};
  std::int32_t origin_y{0};
};

[[nodiscard]] std::optional<TargetView> resolve_target(const Document& document, StrokePatch::Target target,
                                                       LayerId layer_id, ChannelId channel_id) noexcept {
  switch (target) {
    case StrokePatch::Target::LayerContent: {
      const auto* layer = document.find_layer(layer_id);
      if (layer == nullptr) {
        return std::nullopt;
      }
      return TargetView{&layer->pixels(), layer->bounds().x, layer->bounds().y};
    }
    case StrokePatch::Target::LayerMask: {
      const auto* layer = document.find_layer(layer_id);
      if (layer == nullptr || !layer->mask().has_value()) {
        return std::nullopt;
      }
      const auto& mask = *layer->mask();
      return TargetView{&mask.pixels, mask.bounds.x, mask.bounds.y};
    }
    case StrokePatch::Target::DocumentChannel: {
      const auto* channel = document.find_channel(channel_id);
      if (channel == nullptr) {
        return std::nullopt;
      }
      return TargetView{&channel->pixels(), 0, 0};
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool same_geometry(const PixelBuffer& a, const PixelBuffer& b) noexcept {
  return a.width() == b.width() && a.height() == b.height() && a.format() == b.format();
}

[[nodiscard]] Rect intersect(Rect a, Rect b) noexcept {
  const auto left = std::max(a.x, b.x);
  const auto top = std::max(a.y, b.y);
  const auto right = std::min(a.x + a.width, b.x + b.width);
  const auto bottom = std::min(a.y + a.height, b.y + b.height);
  if (right <= left || bottom <= top) {
    return Rect{};
  }
  return Rect{left, top, right - left, bottom - top};
}

[[nodiscard]] bool rect_inside(Rect rect, const PixelBuffer& buffer) noexcept {
  return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x + rect.width <= buffer.width() &&
         rect.y + rect.height <= buffer.height();
}

}  // namespace

const PixelBuffer* stroke_patch_buffer(const Document& document, const StrokePatch& patch) noexcept {
  const auto view = resolve_target(document, patch.target, patch.layer_id, patch.channel_id);
  return view.has_value() ? view->buffer : nullptr;
}

PixelBuffer* stroke_patch_buffer(Document& document, const StrokePatch& patch) noexcept {
  switch (patch.target) {
    case StrokePatch::Target::LayerContent: {
      auto* layer = document.find_layer(patch.layer_id);
      return layer != nullptr ? &layer->pixels() : nullptr;
    }
    case StrokePatch::Target::LayerMask: {
      auto* layer = document.find_layer(patch.layer_id);
      if (layer == nullptr || !std::as_const(*layer).mask().has_value()) {
        return nullptr;
      }
      return &layer->mask()->pixels;
    }
    case StrokePatch::Target::DocumentChannel: {
      auto* channel = document.find_channel(patch.channel_id);
      return channel != nullptr ? &channel->pixels() : nullptr;
    }
  }
  return nullptr;
}

PixelBuffer copy_pixel_rect(const PixelBuffer& source, Rect rect) {
  PixelBuffer copy(rect.width, rect.height, source.format());
  const auto pixel_bytes = bytes_per_pixel(source.format());
  const auto row_bytes = static_cast<std::size_t>(rect.width) * pixel_bytes;
  const auto x_offset = static_cast<std::size_t>(rect.x) * pixel_bytes;
  // One data() call detaches (nothing to detach on a fresh buffer) before the row loop.
  auto target_bytes = copy.data();
  const auto target_stride = copy.stride_bytes();
  for (std::int32_t y = 0; y < rect.height; ++y) {
    const auto source_row = source.row(rect.y + y);
    std::memcpy(target_bytes.data() + static_cast<std::size_t>(y) * target_stride, source_row.data() + x_offset,
                row_bytes);
  }
  return copy;
}

void write_pixel_rect(PixelBuffer& target, Rect rect, const PixelBuffer& source) {
  const auto pixel_bytes = bytes_per_pixel(target.format());
  const auto row_bytes = static_cast<std::size_t>(rect.width) * pixel_bytes;
  const auto x_offset = static_cast<std::size_t>(rect.x) * pixel_bytes;
  // Detach once up front; the row loop then writes into uniquely owned storage.
  auto target_bytes = target.data();
  const auto target_stride = target.stride_bytes();
  for (std::int32_t y = 0; y < rect.height; ++y) {
    const auto source_row = source.row(y);
    std::memcpy(target_bytes.data() + static_cast<std::size_t>(rect.y + y) * target_stride + x_offset,
                source_row.data(), row_bytes);
  }
}

std::optional<StrokePatch> capture_stroke_patch(const Document& before, const Document& after,
                                                StrokePatch::Target target, LayerId layer_id, ChannelId channel_id,
                                                Rect document_dirty) {
  const auto before_view = resolve_target(before, target, layer_id, channel_id);
  const auto after_view = resolve_target(after, target, layer_id, channel_id);
  if (!before_view.has_value() || !after_view.has_value()) {
    return std::nullopt;
  }
  if (before_view->origin_x != after_view->origin_x || before_view->origin_y != after_view->origin_y ||
      !same_geometry(*before_view->buffer, *after_view->buffer) || before_view->buffer->empty()) {
    return std::nullopt;
  }

  // Everything the stroke changed must be the target buffer: every other buffer
  // in the live document still shares its copy-on-write storage with the
  // snapshot. Buffer counts match too (a stroke adds and removes nothing).
  PixelStorageSet snapshot_storage;
  collect_pixel_storage(before, snapshot_storage);
  std::size_t before_count = 0;
  visit_pixel_buffers(before, [&before_count](const PixelBuffer&) { ++before_count; });
  std::size_t after_count = 0;
  std::size_t foreign = 0;
  bool target_detached = false;
  visit_pixel_buffers(after, [&](const PixelBuffer& buffer) {
    ++after_count;
    if (buffer.byte_size() == 0 || snapshot_storage.count(buffer.data().data()) != 0) {
      return;
    }
    ++foreign;
    if (&buffer == after_view->buffer) {
      target_detached = true;
    }
  });
  if (before_count != after_count || foreign != 1 || !target_detached) {
    // foreign == 0 means no pixel was written: the snapshot costs nothing, keep it.
    return std::nullopt;
  }

  const Rect local_dirty{document_dirty.x - before_view->origin_x, document_dirty.y - before_view->origin_y,
                         document_dirty.width, document_dirty.height};
  const auto rect = intersect(local_dirty, Rect::from_size(before_view->buffer->width(), before_view->buffer->height()));
  if (rect.empty()) {
    return std::nullopt;
  }
  StrokePatch patch;
  patch.target = target;
  patch.layer_id = layer_id;
  patch.channel_id = channel_id;
  patch.rect = rect;
  patch.document_rect = Rect{rect.x + before_view->origin_x, rect.y + before_view->origin_y, rect.width, rect.height};
  patch.before = copy_pixel_rect(*before_view->buffer, rect);
  return patch;
}

bool apply_stroke_patch(Document& live, StrokePatch& patch, bool restore_before) {
  // Geometry check through const access first: a mismatch must not bump revisions.
  const auto* current = stroke_patch_buffer(std::as_const(live), patch);
  if (current == nullptr || current->format() != patch.before.format() || !rect_inside(patch.rect, *current)) {
    return false;
  }
  if (restore_before && patch.after.empty()) {
    patch.after = copy_pixel_rect(*current, patch.rect);
  }
  const auto& source = restore_before ? patch.before : patch.after;
  if (source.empty()) {
    return false;
  }
  auto* buffer = stroke_patch_buffer(live, patch);
  if (buffer == nullptr) {
    return false;
  }
  write_pixel_rect(*buffer, patch.rect, source);
  return true;
}

}  // namespace patchy
