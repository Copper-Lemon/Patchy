#include "formats/jp2_document_io.hpp"

#include "core/pixel_depth.hpp"
#include "formats/document_flatten.hpp"
#include "render/compositor.hpp"
#include "support/string_utils.hpp"
#include "support/translate_noop.hpp"

#include "openjpeg.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace patchy::jp2 {

namespace {

constexpr std::int32_t kMaxSide = 65535;

struct ReadStream {
  std::span<const std::uint8_t> bytes;
  std::size_t position{0};
};

OPJ_SIZE_T read_bytes(void* buffer, OPJ_SIZE_T count, void* user) {
  auto* stream = static_cast<ReadStream*>(user);
  if (stream->position >= stream->bytes.size()) {
    return static_cast<OPJ_SIZE_T>(-1);
  }
  const auto available = std::min<std::size_t>(count, stream->bytes.size() - stream->position);
  std::memcpy(buffer, stream->bytes.data() + stream->position, available);
  stream->position += available;
  return available;
}

OPJ_OFF_T skip_bytes(OPJ_OFF_T count, void* user) {
  auto* stream = static_cast<ReadStream*>(user);
  if (count < 0 || stream->position + static_cast<std::size_t>(count) > stream->bytes.size()) {
    return -1;
  }
  stream->position += static_cast<std::size_t>(count);
  return count;
}

OPJ_BOOL seek_bytes(OPJ_OFF_T offset, void* user) {
  auto* stream = static_cast<ReadStream*>(user);
  if (offset < 0 || static_cast<std::size_t>(offset) > stream->bytes.size()) {
    return OPJ_FALSE;
  }
  stream->position = static_cast<std::size_t>(offset);
  return OPJ_TRUE;
}

struct WriteStream {
  std::vector<std::uint8_t> bytes;
  std::size_t position{0};
};

OPJ_SIZE_T write_bytes(void* buffer, OPJ_SIZE_T count, void* user) {
  auto* stream = static_cast<WriteStream*>(user);
  if (stream->position + count > stream->bytes.size()) {
    stream->bytes.resize(stream->position + count);
  }
  std::memcpy(stream->bytes.data() + stream->position, buffer, count);
  stream->position += count;
  return count;
}

OPJ_OFF_T skip_write(OPJ_OFF_T count, void* user) {
  auto* stream = static_cast<WriteStream*>(user);
  if (count < 0) {
    return -1;
  }
  stream->position += static_cast<std::size_t>(count);
  if (stream->position > stream->bytes.size()) {
    stream->bytes.resize(stream->position);
  }
  return count;
}

OPJ_BOOL seek_write(OPJ_OFF_T offset, void* user) {
  auto* stream = static_cast<WriteStream*>(user);
  if (offset < 0) {
    return OPJ_FALSE;
  }
  stream->position = static_cast<std::size_t>(offset);
  if (stream->position > stream->bytes.size()) {
    stream->bytes.resize(stream->position);
  }
  return OPJ_TRUE;
}

void record_error(const char* message, void* user) {
  auto* text = static_cast<std::string*>(user);
  if (text->empty() && message != nullptr) {
    *text = message;
    while (!text->empty() && (text->back() == '\n' || text->back() == '\r')) {
      text->pop_back();
    }
  }
}

void ignore_message(const char* /*message*/, void* /*user*/) {}

struct StreamDeleter {
  void operator()(opj_stream_t* stream) const noexcept { opj_stream_destroy(stream); }
};
struct CodecDeleter {
  void operator()(opj_codec_t* codec) const noexcept { opj_destroy_codec(codec); }
};
struct ImageDeleter {
  void operator()(opj_image_t* image) const noexcept { opj_image_destroy(image); }
};

[[nodiscard]] std::runtime_error failure(const char* noop_message, const std::string& detail) {
  std::string message = noop_message;
  if (!detail.empty()) {
    message += ": ";
    message += detail;
  }
  return std::runtime_error(message);
}


[[nodiscard]] bool is_jp2_container(std::span<const std::uint8_t> bytes) noexcept {
  static const std::uint8_t kSignature[] = {0, 0, 0, 0x0c, 'j', 'P', ' ', ' ', 0x0d, 0x0a, 0x87, 0x0a};
  return bytes.size() >= sizeof(kSignature) && std::memcmp(bytes.data(), kSignature, sizeof(kSignature)) == 0;
}

[[nodiscard]] bool is_j2k_codestream(std::span<const std::uint8_t> bytes) noexcept {
  return bytes.size() >= 4 && bytes[0] == 0xff && bytes[1] == 0x4f && bytes[2] == 0xff && bytes[3] == 0x51;
}

// sYCC (JFIF-style full-range YCbCr) to RGB on 0..max integers.
void sycc_to_rgb(int y, int cb, int cr, int max, int& r, int& g, int& b) noexcept {
  const float half = static_cast<float>(max + 1) / 2.0F;
  const float yf = static_cast<float>(y);
  const float cbf = static_cast<float>(cb) - half;
  const float crf = static_cast<float>(cr) - half;
  r = std::clamp(static_cast<int>(std::lround(yf + 1.402F * crf)), 0, max);
  g = std::clamp(static_cast<int>(std::lround(yf - 0.344136F * cbf - 0.714136F * crf)), 0, max);
  b = std::clamp(static_cast<int>(std::lround(yf + 1.772F * cbf)), 0, max);
}

}  // namespace

const std::vector<std::string>& jp2_extensions() {
  static const std::vector<std::string> extensions = {"jp2", "j2k", "j2c", "jpc", "jpx", "jpf"};
  return extensions;
}

bool is_jp2_extension(std::string_view extension) {
  const auto normalized = normalized_extension(extension, false);
  const auto& extensions = jp2_extensions();
  return std::find(extensions.begin(), extensions.end(), normalized) != extensions.end();
}

bool sniff(std::span<const std::uint8_t> bytes) noexcept {
  return is_jp2_container(bytes) || is_j2k_codestream(bytes);
}

FormatReadResult read_jp2(std::span<const std::uint8_t> bytes) {
  if (!sniff(bytes)) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "This file is not a JPEG 2000 image"));
  }
  std::string error;
  std::unique_ptr<opj_codec_t, CodecDeleter> codec(
      opj_create_decompress(is_jp2_container(bytes) ? OPJ_CODEC_JP2 : OPJ_CODEC_J2K));
  if (!codec) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 decoder could not be created"));
  }
  opj_set_error_handler(codec.get(), record_error, &error);
  opj_set_warning_handler(codec.get(), ignore_message, nullptr);
  opj_set_info_handler(codec.get(), ignore_message, nullptr);
  opj_dparameters_t parameters;
  opj_set_default_decoder_parameters(&parameters);
  if (!opj_setup_decoder(codec.get(), &parameters)) {
    throw failure(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 decoder could not be created"), error);
  }
  ReadStream source{bytes, 0};
  std::unique_ptr<opj_stream_t, StreamDeleter> stream(opj_stream_default_create(OPJ_TRUE));
  opj_stream_set_read_function(stream.get(), read_bytes);
  opj_stream_set_skip_function(stream.get(), skip_bytes);
  opj_stream_set_seek_function(stream.get(), seek_bytes);
  opj_stream_set_user_data(stream.get(), &source, nullptr);
  opj_stream_set_user_data_length(stream.get(), static_cast<OPJ_UINT64>(bytes.size()));

  opj_image_t* raw_image = nullptr;
  if (!opj_read_header(stream.get(), codec.get(), &raw_image)) {
    opj_image_destroy(raw_image);
    throw failure(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 file is damaged or truncated"), error);
  }
  std::unique_ptr<opj_image_t, ImageDeleter> image(raw_image);
  if (!opj_decode(codec.get(), stream.get(), image.get()) || !opj_end_decompress(codec.get(), stream.get())) {
    throw failure(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 image could not be decoded"), error);
  }
  const auto components = image->numcomps;
  if (components == 0 || components > 4 || image->comps == nullptr) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "This JPEG 2000 image has a channel layout Patchy does not read"));
  }
  const auto& first = image->comps[0];
  // Up to 32-bit components (the corpus's u4/i4 files); anything over 8 bits opens as 16.
  if (first.w == 0 || first.h == 0 || first.w > static_cast<OPJ_UINT32>(kMaxSide) ||
      first.h > static_cast<OPJ_UINT32>(kMaxSide) || first.data == nullptr || first.prec == 0 || first.prec > 32) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "This JPEG 2000 image has a channel layout Patchy does not read"));
  }
  const auto width = static_cast<std::int32_t>(first.w);
  const auto height = static_cast<std::int32_t>(first.h);
  for (OPJ_UINT32 c = 1; c < components; ++c) {
    const auto& comp = image->comps[c];
    if (comp.data == nullptr || comp.w == 0 || comp.h == 0 || comp.prec == 0 || comp.prec > 32) {
      throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "This JPEG 2000 image has a channel layout Patchy does not read"));
    }
  }
  const bool deep = first.prec > 8;
  const auto depth = deep ? BitDepth::UInt16 : BitDepth::UInt8;
  const int out_max = deep ? 65535 : 255;
  // Component value at full-resolution pixel (x, y): subsampled planes repeat samples.
  const auto sample = [&](OPJ_UINT32 c, std::int32_t x, std::int32_t y) -> int {
    const auto& comp = image->comps[c];
    const auto sx = std::min<OPJ_UINT32>(comp.w - 1, static_cast<OPJ_UINT32>(x) / std::max<OPJ_UINT32>(1, comp.dx));
    const auto sy = std::min<OPJ_UINT32>(comp.h - 1, static_cast<OPJ_UINT32>(y) / std::max<OPJ_UINT32>(1, comp.dy));
    // OpenJPEG hands over 32-bit components as unsigned bit patterns in an int; widen before
    // any arithmetic so the full range survives.
    std::int64_t v = static_cast<std::uint32_t>(comp.data[static_cast<std::size_t>(sy) * comp.w + sx]);
    if (comp.sgnd) {
      v = static_cast<std::int32_t>(static_cast<std::uint32_t>(v));  // signed pattern
      v += std::int64_t{1} << (comp.prec - 1);
    }
    const std::int64_t comp_max = (std::int64_t{1} << comp.prec) - 1;
    v = std::clamp<std::int64_t>(v, 0, comp_max);
    if (comp_max != out_max) {
      v = (v * out_max + comp_max / 2) / comp_max;
    }
    return static_cast<int>(v);
  };
  const bool ycc = image->color_space == OPJ_CLRSPC_SYCC || image->color_space == OPJ_CLRSPC_EYCC ||
                   (image->color_space == OPJ_CLRSPC_UNKNOWN && components >= 3 &&
                    (image->comps[1].dx > 1 || image->comps[1].dy > 1));
  const bool cmyk = image->color_space == OPJ_CLRSPC_CMYK && components == 4;
  const bool has_alpha = !cmyk && (components == 2 || components == 4);

  PixelBuffer pixels(width, height, with_bit_depth(PixelFormat::rgba8(), depth));
  for (std::int32_t y = 0; y < height; ++y) {
    auto row = pixels.row(y);
    for (std::int32_t x = 0; x < width; ++x) {
      int r = 0;
      int g = 0;
      int b = 0;
      int a = out_max;
      if (components <= 2) {
        r = g = b = sample(0, x, y);
        if (components == 2) {
          a = sample(1, x, y);
        }
      } else if (cmyk) {
        const int c = sample(0, x, y);
        const int m = sample(1, x, y);
        const int yy = sample(2, x, y);
        const int k = sample(3, x, y);
        r = (out_max - c) * (out_max - k) / out_max;
        g = (out_max - m) * (out_max - k) / out_max;
        b = (out_max - yy) * (out_max - k) / out_max;
      } else {
        r = sample(0, x, y);
        g = sample(1, x, y);
        b = sample(2, x, y);
        if (ycc) {
          sycc_to_rgb(r, g, b, out_max, r, g, b);
        }
        if (components == 4) {
          a = sample(3, x, y);
        }
      }
      const auto at = static_cast<std::size_t>(x) * 4U;
      if (deep) {
        auto* out = reinterpret_cast<std::uint16_t*>(row.data());
        out[at] = static_cast<std::uint16_t>(r);
        out[at + 1] = static_cast<std::uint16_t>(g);
        out[at + 2] = static_cast<std::uint16_t>(b);
        out[at + 3] = static_cast<std::uint16_t>(a);
      } else {
        row[at] = static_cast<std::uint8_t>(r);
        row[at + 1] = static_cast<std::uint8_t>(g);
        row[at + 2] = static_cast<std::uint8_t>(b);
        row[at + 3] = static_cast<std::uint8_t>(a);
      }
    }
  }
  FormatReadResult result{Document(width, height, with_bit_depth(PixelFormat::rgba8(), depth)), {}};
  result.document.color_state().bit_depth = depth;
  if (image->icc_profile_buf != nullptr && image->icc_profile_len > 0) {
    result.document.color_state().embedded_icc_profile.assign(image->icc_profile_buf,
                                                              image->icc_profile_buf + image->icc_profile_len);
  }
  if (!has_alpha && components != 2) {
    // Opaque sources keep alpha at full; nothing to do, the loop wrote out_max.
  }
  result.document.add_pixel_layer("Background", std::move(pixels));
  if (cmyk) {
    result.notices.emplace_back(PATCHY_TRANSLATE_NOOP("QObject", "CMYK JPEG 2000 image: converted to RGB with a plain ink mix"));
  }
  return result;
}

std::vector<std::uint8_t> write_jp2(const Document& document) {
  const auto doc_depth = document.color_state().bit_depth;
  const bool deep = doc_depth == BitDepth::UInt16;
  const PixelBuffer flat = deep ? Compositor{}.flatten_rgba_deep(document) : flatten_document_rgba8(document);
  const auto w = flat.width();
  const auto h = flat.height();
  if (w <= 0 || h <= 0) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "Cannot write an empty image as JPEG 2000"));
  }
  const auto channels = flat.format().channels;
  const auto flat_depth = flat.format().bit_depth;
  const int prec = deep ? 16 : 8;
  // Alpha goes along only when some pixel is not opaque.
  bool any_alpha = false;
  for (std::int32_t y = 0; y < h && !any_alpha && channels == 4; ++y) {
    const auto row = flat.row(y);
    for (std::int32_t x = 0; x < w; ++x) {
      const auto index = static_cast<std::size_t>(x) * 4U + 3U;
      const bool opaque = flat_depth == BitDepth::UInt16 ? reinterpret_cast<const std::uint16_t*>(row.data())[index] == 65535
                          : flat_depth == BitDepth::Float32 ? reinterpret_cast<const float*>(row.data())[index] >= 1.0F
                                                            : row[index] == 255;
      if (!opaque) {
        any_alpha = true;
        break;
      }
    }
  }
  const OPJ_UINT32 components = any_alpha ? 4 : 3;
  opj_image_cmptparm_t params[4];
  std::memset(params, 0, sizeof(params));
  for (OPJ_UINT32 c = 0; c < components; ++c) {
    params[c].dx = 1;
    params[c].dy = 1;
    params[c].w = static_cast<OPJ_UINT32>(w);
    params[c].h = static_cast<OPJ_UINT32>(h);
    params[c].prec = static_cast<OPJ_UINT32>(prec);
    params[c].sgnd = 0;
  }
  std::unique_ptr<opj_image_t, ImageDeleter> image(opj_image_create(components, params, OPJ_CLRSPC_SRGB));
  if (!image) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 file could not be written"));
  }
  image->x0 = 0;
  image->y0 = 0;
  image->x1 = static_cast<OPJ_UINT32>(w);
  image->y1 = static_cast<OPJ_UINT32>(h);
  if (any_alpha) {
    image->comps[3].alpha = 1;
  }
  for (std::int32_t y = 0; y < h; ++y) {
    const auto row = flat.row(y);
    for (std::int32_t x = 0; x < w; ++x) {
      for (OPJ_UINT32 c = 0; c < components; ++c) {
        const auto index = static_cast<std::size_t>(x) * channels + c;
        int v = 0;
        switch (flat_depth) {
          case BitDepth::UInt8: v = row[index]; break;
          case BitDepth::UInt16: v = reinterpret_cast<const std::uint16_t*>(row.data())[index]; break;
          case BitDepth::Float32:
            v = static_cast<int>(std::lround(std::clamp(reinterpret_cast<const float*>(row.data())[index], 0.0F, 1.0F) * 255.0F));
            break;
        }
        image->comps[c].data[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = v;
      }
    }
  }
  opj_cparameters_t parameters;
  opj_set_default_encoder_parameters(&parameters);
  parameters.tcp_numlayers = 1;
  parameters.tcp_rates[0] = 0;  // lossless
  parameters.cp_disto_alloc = 1;
  parameters.irreversible = 0;
  int resolutions = 1;
  while (resolutions < 6 && (std::min(w, h) >> resolutions) >= 1) {
    ++resolutions;
  }
  parameters.numresolution = resolutions;
  std::string error;
  std::unique_ptr<opj_codec_t, CodecDeleter> codec(opj_create_compress(OPJ_CODEC_JP2));
  if (!codec) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 file could not be written"));
  }
  opj_set_error_handler(codec.get(), record_error, &error);
  opj_set_warning_handler(codec.get(), ignore_message, nullptr);
  opj_set_info_handler(codec.get(), ignore_message, nullptr);
  if (!opj_setup_encoder(codec.get(), &parameters, image.get())) {
    throw failure(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 file could not be written"), error);
  }
  WriteStream sink;
  std::unique_ptr<opj_stream_t, StreamDeleter> stream(opj_stream_default_create(OPJ_FALSE));
  opj_stream_set_write_function(stream.get(), write_bytes);
  opj_stream_set_skip_function(stream.get(), skip_write);
  opj_stream_set_seek_function(stream.get(), seek_write);
  opj_stream_set_user_data(stream.get(), &sink, nullptr);
  if (!opj_start_compress(codec.get(), image.get(), stream.get()) || !opj_encode(codec.get(), stream.get()) ||
      !opj_end_compress(codec.get(), stream.get())) {
    throw failure(PATCHY_TRANSLATE_NOOP("QObject", "The JPEG 2000 file could not be written"), error);
  }
  stream.reset();  // flushes
  return std::move(sink.bytes);
}

}  // namespace patchy::jp2
