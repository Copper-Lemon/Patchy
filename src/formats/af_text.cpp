#include "formats/af_import_internal.hpp"

#include "core/environment.hpp"
#include "core/layer_metadata.hpp"
#include "psd/psd_text_runs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace patchy::af::detail {
namespace {

// Text nodes (TxtA artistic / TxtF frame): extract the story into a Patchy
// text layer carrying the standard patchy.text.* metadata plus the .af
// placement markers; MainWindow renders it post-open through the internal
// text pipeline (the SVG import pattern). Wire shape (pinned by the corpus
// text docs): StSt -> Stry -> Blok[] (StBl) with Glyp/GStr/Utf8 = the text
// (NUL-terminated per block), GAtt/GlAS/Runs[] (GlAR) = glyph style runs
// whose Indx is the run's END boundary (exclusive, Unicode CODEPOINTS of the
// block text including the NUL - pinned by the emoji fixture; the runs
// partition it) and whose Item (possibly a shared-class ref, possibly sparse
// - unset fields inherit the previous run) carries DFnt font, Doub[0] size,
// Objs[0] = brush FDsc -> FDeF -> Colr RGBA; PAtt/PaAS/Runs[] the same
// END-boundary shape for paragraphs
// (Item.Ints[0] = alignment), TxtH -> ArFr|CoFr -> FrmB = the layout box
// ([x0,y0,x1,y1]; ArtV = artistic ascent). Mixed run styles become
// patchy.text.runs/html through the shared PSD serializers. Returns nullopt
// when the story shape is missing (caller keeps the placeholder path).

// Decode one UTF-8 codepoint at `index` of `text`; lenient (malformed bytes
// decode one byte as U+003F, matching the PSD reader's tolerance).
[[nodiscard]] std::uint32_t decode_utf8_at(const std::string& text, std::size_t index,
                                           std::size_t& consumed) {
  const auto lead = static_cast<unsigned char>(text[index]);
  consumed = 1;
  if (lead < 0x80U) {
    return lead;
  }
  if ((lead & 0xE0U) == 0xC0U && index + 1 < text.size()) {
    consumed = 2;
    return ((lead & 0x1FU) << 6U) | (static_cast<unsigned char>(text[index + 1]) & 0x3FU);
  }
  if ((lead & 0xF0U) == 0xE0U && index + 2 < text.size()) {
    consumed = 3;
    return ((lead & 0x0FU) << 12U) |
           ((static_cast<unsigned char>(text[index + 1]) & 0x3FU) << 6U) |
           (static_cast<unsigned char>(text[index + 2]) & 0x3FU);
  }
  if ((lead & 0xF8U) == 0xF0U && index + 3 < text.size()) {
    consumed = 4;
    return ((lead & 0x07U) << 18U) |
           ((static_cast<unsigned char>(text[index + 1]) & 0x3FU) << 12U) |
           ((static_cast<unsigned char>(text[index + 2]) & 0x3FU) << 6U) |
           (static_cast<unsigned char>(text[index + 3]) & 0x3FU);
  }
  return 0x3FU;
}

// Apply a glyph-run Item's style fields onto `run` (fields the item omits keep
// the carried-in values, so sparse items inherit from the previous run).
// Caps state of a glyph-run item, from the OtAt OpenType-feature settings in
// the item's Objs (Setn[] OTFS {Feat 4CC, Valu}): the private 'CAP\x01' tag is
// All Caps (Patchy uppercases the covered text); smcp/c2sc/pcap/c2pc/titl/unic
// are the small-caps family (unsupported; notice). An item WITHOUT an OtAt is
// sparse and inherits the previous run's caps; OtAt with an empty Setn resets.
enum class AfRunCaps { Inherit, None, AllCaps, Unsupported };

[[nodiscard]] AfRunCaps af_run_caps(const af::AfClass& item) {
  const auto* objects = class_list(item, af::tag4("Objs"));
  if (objects == nullptr) {
    return AfRunCaps::Inherit;
  }
  for (const auto& object : *objects) {
    if (object == nullptr || object->type_tag != af::tag4("OtAt")) {
      continue;
    }
    AfRunCaps result = AfRunCaps::None;
    if (const auto* settings = class_list(*object, af::tag4("Setn"))) {
      for (const auto& setting : *settings) {
        if (setting == nullptr || setting->int_field(af::tag4("Valu"), 0) == 0) {
          continue;
        }
        const auto feature =
            static_cast<std::uint32_t>(setting->int_field(af::tag4("Feat"), 0));
        if (feature == af::tag4("CAP\x01")) {
          return AfRunCaps::AllCaps;
        }
        if (feature == af::tag4("smcp") || feature == af::tag4("c2sc") ||
            feature == af::tag4("pcap") || feature == af::tag4("c2pc") ||
            feature == af::tag4("titl") || feature == af::tag4("unic")) {
          result = AfRunCaps::Unsupported;
        }
      }
    }
    return result;
  }
  return AfRunCaps::Inherit;
}

// Uppercase a UTF-8 byte range in place, byte-length-preserving: ASCII a-z and
// the Latin-1 letters (except ss, whose uppercase would grow). Enough for the
// All Caps import; anything outside stays as authored.
void uppercase_utf8_range(std::string& text, std::size_t from, std::size_t to) {
  to = std::min(to, text.size());
  for (std::size_t i = from; i < to; ++i) {
    const auto byte = static_cast<unsigned char>(text[i]);
    if (byte >= 'a' && byte <= 'z') {
      text[i] = static_cast<char>(byte - 0x20);
    } else if (byte == 0xC3U && i + 1 < to) {
      const auto low = static_cast<unsigned char>(text[i + 1]);
      // U+00E0..U+00FE lowercase (0xC3 0xA0..0xBE) -> U+00C0..U+00DE
      // (0xC3 0x80..0x9E); skip the division sign U+00F7.
      if (low >= 0xA0U && low <= 0xBEU && low != 0xB7U) {
        text[i + 1] = static_cast<char>(low - 0x20);
      }
      ++i;
    } else if (byte >= 0xC2U) {
      // Skip over the rest of any other multi-byte sequence.
      i += byte >= 0xF0U ? 3 : (byte >= 0xE0U ? 2 : 1);
    }
  }
}

void apply_af_run_item(const af::AfClass& item, psd::PsdTextStyleRun& run, float& alpha) {
  if (const af::AfClass* font = item.child_class(af::tag4("DFnt"))) {
    const std::string stored = font->string_field(af::tag4("Famy"));
    if (!stored.empty()) {
      run.family = stored;
    }
    // Non-normal width classes (Widh 5 = normal) can lose the face through
    // the wire family alone ("Arial" + Widh 3 is Arial Narrow); the
    // PostScript name resolves the display family ("ArialNarrow" -> "Arial
    // Narrow"). Upgrade ONLY when the resolved name extends the wire family
    // with a qualifier - face-specific wire families ("Futura MdCn BT") are
    // already the installed name and a humanized PostScript would miss.
    if (font->int_field(af::tag4("Widh"), 5) != 5) {
      const std::string postscript = font->string_field(af::tag4("Post"));
      if (!postscript.empty()) {
        auto resolved = psd::heuristic_resolved_photoshop_font(postscript);
        const auto ci_equal = [](char a, char b) {
          return std::tolower(static_cast<unsigned char>(a)) ==
                 std::tolower(static_cast<unsigned char>(b));
        };
        const bool extends_family =
            resolved.family.size() > run.family.size() + 1 &&
            resolved.family[run.family.size()] == ' ' &&
            std::equal(run.family.begin(), run.family.end(), resolved.family.begin(), ci_equal);
        if (extends_family) {
          run.family = std::move(resolved.family);
        }
      }
    }
    run.bold = font->int_field(af::tag4("Wegt"), 400) >= 600;
    run.italic = font->bool_field(af::tag4("Ital"), false);
  }
  const auto doubles = item.vec_field(af::tag4("Doub"));
  if (!doubles.empty() && doubles[0] > 0.0) {
    run.size = doubles[0];
  }
  if (const auto* fills = class_list(item, af::tag4("Objs"));
      fills != nullptr && !fills->empty() && fills->front() != nullptr) {
    if (const af::AfClass* fill = fills->front()->child_class(af::tag4("FDeF"))) {
      if (const auto stored = read_rgba_color(fill->child_class(af::tag4("Colr")))) {
        const auto to_channel = [](float value) {
          return static_cast<std::uint8_t>(
              std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
        };
        run.color = RgbColor{to_channel((*stored)[0]), to_channel((*stored)[1]),
                             to_channel((*stored)[2])};
        alpha = std::clamp((*stored)[3], 0.0F, 1.0F);
      }
    }
  }
}

// Affinity ParagraphAlignXType (0 left, 1 centre, 2 right, 3 justify) -> the
// PSD justification ids the paragraph-runs serializer expects (1 right,
// 2 center, 3 justify).
[[nodiscard]] int psd_justification_from_af_alignment(std::int64_t align) {
  switch (align) {
    case 1:
      return 2;
    case 2:
      return 1;
    case 3:
      return 3;
    default:
      return 0;
  }
}

}  // namespace
[[nodiscard]] std::optional<Layer> build_text_layer(LayerBuildContext& ctx, const af::AfClass& node,
                                                    const std::string& display) {
  const af::AfClass* story = node.child_class(af::tag4("StSt"));
  if (story == nullptr) {
    return std::nullopt;
  }
  const auto* blocks = class_list(*story, af::tag4("Blok"));
  if (blocks == nullptr || blocks->empty()) {
    return std::nullopt;
  }

  std::string text;
  std::vector<psd::PsdTextStyleRun> style_runs;
  std::vector<psd::PsdTextParagraphRun> paragraph_runs;
  bool has_style = false;
  float first_alpha = 1.0F;
  std::int64_t align = -1;
  // Carried style: runs inherit unset fields from the previous run; the
  // pre-first-run default matches the old single-style fallbacks.
  psd::PsdTextStyleRun carry;
  carry.family = "Arial";
  carry.size = 12.0;
  carry.color = RgbColor{0, 0, 0};
  AfRunCaps carry_caps = AfRunCaps::None;
  bool unsupported_caps = false;
  int base_units = 0;  // output UTF-16 units emitted before the current block

  for (const auto& block : *blocks) {
    if (block == nullptr) {
      continue;
    }
    std::string raw;
    if (const af::AfClass* glyphs = block->child_class(af::tag4("Glyp"))) {
      raw = glyphs->string_field(af::tag4("Utf8"));
    }
    if (!text.empty()) {
      text += '\n';
      base_units += 1;
    }

    // Walk the block text once: paragraph breaks (U+2029) and soft line
    // breaks (U+2028) become '\n', NULs drop, and out_at maps every wire
    // CODEPOINT boundary (run Indx values count codepoints of the original
    // block text INCLUDING the trailing NUL) to its output UTF-16 offset
    // (the patchy.text.runs contract).
    std::string out;
    std::vector<int> out_at{0};
    std::vector<std::size_t> out_byte_at{0};  // codepoint boundary -> byte offset in out
    int local_out = 0;
    std::size_t at = 0;
    while (at < raw.size()) {
      std::size_t consumed = 1;
      const std::uint32_t cp = decode_utf8_at(raw, at, consumed);
      if (cp == 0x2029U || cp == 0x2028U) {
        out += '\n';
        local_out += 1;
      } else if (cp != 0U) {
        out.append(raw, at, consumed);
        local_out += cp > 0xFFFFU ? 2 : 1;
      }
      out_at.push_back(local_out);
      out_byte_at.push_back(out.size());
      at += consumed;
    }
    const std::size_t block_text_base = text.size();
    text += out;
    const int block_units = static_cast<int>(out_at.size()) - 1;

    const auto run_range = [&](int from, int to) {
      from = std::clamp(from, 0, block_units);
      to = std::clamp(to, from, block_units);
      return std::pair<int, int>{base_units + out_at[static_cast<std::size_t>(from)],
                                 base_units + out_at[static_cast<std::size_t>(to)]};
    };

    // Glyph style runs. A block without runs extends the carried style.
    const auto* glyph_runs = [&]() -> const std::vector<std::shared_ptr<af::AfClass>>* {
      const af::AfClass* glyph_atts = block->child_class(af::tag4("GAtt"));
      return glyph_atts != nullptr ? class_list(*glyph_atts, af::tag4("Runs")) : nullptr;
    }();
    const auto push_style_run = [&](const psd::PsdTextStyleRun& run) {
      if (run.length <= 0) {
        return;
      }
      if (!style_runs.empty()) {
        auto& last = style_runs.back();
        // The '\n' joining blocks belongs to no wire run; extend the previous
        // run over it so the runs fully cover the text (html emission drops
        // uncovered characters).
        if (last.start + last.length < run.start) {
          last.length = run.start - last.start;
        }
        if (last.family == run.family && last.size == run.size && last.color == run.color &&
            last.bold == run.bold && last.italic == run.italic) {
          last.length = run.start + run.length - last.start;
          return;
        }
      }
      style_runs.push_back(run);
    };
    int prev = 0;
    if (glyph_runs != nullptr) {
      for (const auto& run : *glyph_runs) {
        if (run == nullptr) {
          continue;
        }
        const int end = static_cast<int>(run->int_field(af::tag4("Indx"), block_units));
        psd::PsdTextStyleRun styled = carry;
        if (const af::AfClass* item = run->child_class(af::tag4("Item"))) {
          float alpha = 1.0F;
          apply_af_run_item(*item, styled, alpha);
          if (!has_style) {
            first_alpha = alpha;
          }
          has_style = true;
          if (const auto caps = af_run_caps(*item); caps != AfRunCaps::Inherit) {
            carry_caps = caps;
          }
        }
        if (carry_caps == AfRunCaps::AllCaps) {
          const auto from = static_cast<std::size_t>(std::clamp(prev, 0, block_units));
          const auto to = static_cast<std::size_t>(std::clamp(end, prev, block_units));
          uppercase_utf8_range(text, block_text_base + out_byte_at[from],
                               block_text_base + out_byte_at[to]);
        } else if (carry_caps == AfRunCaps::Unsupported) {
          unsupported_caps = true;
        }
        const auto [start_out, end_out] = run_range(prev, end);
        styled.start = start_out;
        styled.length = end_out - start_out;
        push_style_run(styled);
        carry = styled;
        prev = std::max(prev, end);
      }
    }
    if (prev < block_units) {
      // Tail not covered by any run keeps the carried style.
      if (carry_caps == AfRunCaps::AllCaps) {
        const auto from = static_cast<std::size_t>(std::clamp(prev, 0, block_units));
        uppercase_utf8_range(text, block_text_base + out_byte_at[from],
                             block_text_base + out_byte_at[static_cast<std::size_t>(block_units)]);
      }
      psd::PsdTextStyleRun styled = carry;
      const auto [start_out, end_out] = run_range(prev, block_units);
      styled.start = start_out;
      styled.length = end_out - start_out;
      push_style_run(styled);
    }

    // Paragraph runs (alignment; same END-boundary Indx shape).
    if (const af::AfClass* paragraph_atts = block->child_class(af::tag4("PAtt"))) {
      if (const auto* runs = class_list(*paragraph_atts, af::tag4("Runs"))) {
        int para_prev = 0;
        for (const auto& run : *runs) {
          if (run == nullptr) {
            continue;
          }
          const int end = static_cast<int>(run->int_field(af::tag4("Indx"), block_units));
          std::int64_t run_align = 0;
          double space_before = 0.0;
          double space_after = 0.0;
          double first_line_indent = 0.0;
          double left_indent = 0.0;
          double right_indent = 0.0;
          if (const af::AfClass* item = run->child_class(af::tag4("Item"))) {
            const auto ints = item->vec_field(af::tag4("Ints"));
            if (!ints.empty()) {
              run_align = static_cast<std::int64_t>(ints[0]);
            }
            // Doub[5]/[6] = paragraph space before/after in document px
            // (pinned by the 9/17 spacing probe doc). Doub[2]/[3]/[4] =
            // left/right/first-line indents (the text-indent probe doc):
            // Affinity's left indent positions CONTINUATION lines only, and
            // the first-line indent is ABSOLUTE from the column edge with
            // negatives clamped to 0 at render (tips.psd's PS hanging indent
            // StartIndent 24 / FirstLineIndent -24 converts to wire 24 / 0).
            const auto doubles = item->vec_field(af::tag4("Doub"));
            if (doubles.size() > 6) {
              space_before = std::clamp(doubles[5], 0.0, 10000.0);
              space_after = std::clamp(doubles[6], 0.0, 10000.0);
            }
            if (doubles.size() > 3) {
              left_indent = std::clamp(doubles[2], 0.0, 10000.0);
              right_indent = std::clamp(doubles[3], 0.0, 10000.0);
            }
            if (doubles.size() > 4) {
              first_line_indent = std::clamp(doubles[4], 0.0, 10000.0);
            }
          }
          if (align < 0) {
            align = run_align;
          }
          const auto [start_out, end_out] = run_range(para_prev, end);
          psd::PsdTextParagraphRun paragraph;
          paragraph.start = start_out;
          paragraph.length = end_out - start_out;
          paragraph.justification = psd_justification_from_af_alignment(run_align);
          // Affinity does not push the first paragraph down by its
          // space-before (Qt's top margin would), so the leading paragraph
          // keeps only its space-after.
          paragraph.space_before = paragraph.start == 0 ? 0.0 : space_before;
          paragraph.space_after = space_after;
          // Serialized in the PS/Qt convention: first-line relative to the
          // left indent, so a wire hanging indent goes negative.
          paragraph.first_line_indent = first_line_indent - left_indent;
          paragraph.start_indent = left_indent;
          paragraph.end_indent = right_indent;
          if (paragraph.length > 0) {
            paragraph_runs.push_back(paragraph);
          }
          para_prev = std::max(para_prev, end);
        }
      }
    }

    base_units += local_out;
  }
  const bool only_whitespace =
      text.find_first_not_of(" \t\r\n") == std::string::npos;
  if (only_whitespace || !has_style || style_runs.empty()) {
    return std::nullopt;
  }

  // Layer-level style (options-bar defaults, single-style render path): the
  // first run, exactly like the PSD reader.
  const std::string family = style_runs.front().family;
  const bool bold = style_runs.front().bold;
  const bool italic = style_runs.front().italic;
  const int size = std::clamp(static_cast<int>(std::lround(style_runs.front().size)), 1, 4000);
  const RgbColor first_color = style_runs.front().color;

  // Placement: the TxtH frame box (node-local; Xfrm positions it). For an
  // axis-aligned transform, fold the scale into the font size and map the box
  // through the affine. Rotated/sheared ARTISTIC text instead keeps the raw
  // node-local box and unscaled sizes and carries the full Xfrm in a marker:
  // the post-open pass renders the glyphs THROUGH the affine (crisp, exact)
  // and stamps the standard patchy.text.transform. Rotated FRAME text still
  // approximates axis-aligned (the box-flow renderer has no transform path).
  const af::AfClass* frame = node.child_class(af::tag4("TxtH"));
  if (frame == nullptr) {
    return std::nullopt;
  }
  auto box = frame->vec_field(af::tag4("FrmB"));
  if (box.size() != 4) {
    return std::nullopt;
  }
  double ascent = frame->double_field(af::tag4("ArtV"), 0.0);
  double effective_size = static_cast<double>(size);
  double size_scale = 1.0;
  bool transform_approximated = false;
  std::string transform_marker;
  if (const auto xfrm = node_xfrm(ctx, node); xfrm.size() == 6) {
    const double a = xfrm[0];
    const double b_term = xfrm[1];
    const double tx = xfrm[2];
    const double c_term = xfrm[3];
    const double d = xfrm[4];
    const double ty = xfrm[5];
    const double scale_x = std::hypot(a, c_term);
    const double scale_y = std::hypot(b_term, d);
    const double scale = (scale_x + scale_y) * 0.5;
    if (scale < 0.01) {
      return std::nullopt;
    }
    const double rotation = std::atan2(c_term, a);
    const bool non_axis_aligned =
        std::abs(rotation) > 0.03 || std::abs(scale_x - scale_y) > 0.05 * scale;
    if (non_axis_aligned && node.type_tag == af::tag4("TxtA")) {
      // Full-affine path: raw box, wire-unit sizes, the whole Xfrm rides the
      // marker ("a b tx c d ty", the wire order).
      for (int i = 0; i < 6; ++i) {
        if (i != 0) {
          transform_marker += ' ';
        }
        transform_marker += std::to_string(xfrm[static_cast<std::size_t>(i)]);
      }
    } else {
      // Map the four box corners through the affine; keep the axis-aligned box.
      double min_x = 0.0;
      double min_y = 0.0;
      double max_x = 0.0;
      double max_y = 0.0;
      bool first_corner = true;
      for (const auto& [sx, sy] : {std::pair<double, double>{box[0], box[1]},
                                   {box[2], box[1]},
                                   {box[0], box[3]},
                                   {box[2], box[3]}}) {
        const double px = a * sx + b_term * sy + tx;
        const double py = c_term * sx + d * sy + ty;
        min_x = first_corner ? px : std::min(min_x, px);
        max_x = first_corner ? px : std::max(max_x, px);
        min_y = first_corner ? py : std::min(min_y, py);
        max_y = first_corner ? py : std::max(max_y, py);
        first_corner = false;
      }
      // Assign the four edges in place. box is already known to hold exactly
      // four values, and assigning a braced list instead makes GCC's optimizer
      // misjudge the temporary array it builds for the list, which it reports
      // as a false-positive -Warray-bounds.
      box[0] = min_x;
      box[1] = min_y;
      box[2] = max_x;
      box[3] = max_y;
      effective_size *= scale;
      size_scale = scale;
      ascent *= scale_y;
      transform_approximated = non_axis_aligned;
    }
  }
  const int stored_size =
      std::clamp(static_cast<int>(std::lround(effective_size)), 1, 4000);
  // Fold the transform scale into every run. Rounding the wire size before
  // scaling matches the historical single-style fold (stored_size above), and
  // integral sizes keep the runs serialization at the v1 shape Patchy's own
  // text tool writes.
  for (auto& run : style_runs) {
    run.size = static_cast<double>(std::clamp(
        std::lround(static_cast<double>(std::lround(run.size)) * size_scale), 1L, 4000L));
  }
  for (auto& paragraph : paragraph_runs) {
    paragraph.space_before *= size_scale;
    paragraph.space_after *= size_scale;
    paragraph.first_line_indent *= size_scale;
    paragraph.start_indent *= size_scale;
    paragraph.end_indent *= size_scale;
  }

  Layer layer(ctx.document.allocate_layer_id(), display, LayerKind::Pixel);
  auto& metadata = layer.metadata();
  metadata[kLayerMetadataText] = text;
  metadata[kLayerMetadataTextFont] = family;
  metadata[kLayerMetadataTextSize] = std::to_string(stored_size);
  constexpr char kHex[] = "0123456789abcdef";
  std::string hex = "#......";
  hex[1] = kHex[first_color.red >> 4];
  hex[2] = kHex[first_color.red & 15];
  hex[3] = kHex[first_color.green >> 4];
  hex[4] = kHex[first_color.green & 15];
  hex[5] = kHex[first_color.blue >> 4];
  hex[6] = kHex[first_color.blue & 15];
  metadata[kLayerMetadataTextColor] = hex;
  metadata[kLayerMetadataTextBold] = bold ? "true" : "false";
  metadata[kLayerMetadataTextItalic] = italic ? "true" : "false";
  const bool is_frame = node.type_tag == af::tag4("TxtF");
  if (is_frame) {
    // Frame text wraps within its box (the internal pipeline's box flow).
    metadata[kLayerMetadataTextFlow] = "box";
    metadata[kLayerMetadataTextBoxWidth] = std::to_string(
        std::max(1, static_cast<int>(std::lround(box[2] - box[0]))));
    metadata[kLayerMetadataTextBoxHeight] = std::to_string(
        std::max(1, static_cast<int>(std::lround(box[3] - box[1]))));
  }
  // Mixed run styles and non-left alignment ride the standard PSD-shaped
  // rich-text metadata (runs + Qt html body); a single left-aligned style
  // keeps the plain single-style metadata exactly as before.
  const bool multi_style = style_runs.size() > 1;
  bool any_paragraph_layout = false;
  for (const auto& run : paragraph_runs) {
    any_paragraph_layout = any_paragraph_layout || run.justification != 0 ||
                           run.space_before > 0.0 || run.space_after > 0.0 ||
                           run.first_line_indent != 0.0 || run.start_indent > 0.0 ||
                           run.end_indent > 0.0;
  }
  // Runs also emit for single-style text that carries paragraph layout: the
  // rich-text-runs render path builds REAL paragraph blocks from the plain
  // text, while the html body is a single <p> with <br/> breaks, so block
  // alignment and spacing only apply on the runs path (Affinity's centred
  // multi-paragraph text lost its paragraph spacing through html).
  if (multi_style || any_paragraph_layout) {
    metadata[kLayerMetadataTextRuns] = psd::serialize_patchy_text_runs(style_runs);
  }
  if (any_paragraph_layout) {
    metadata[kLayerMetadataTextParagraphRuns] =
        psd::serialize_patchy_paragraph_runs(paragraph_runs);
  }
  if (multi_style || align > 0) {
    metadata[kLayerMetadataTextHtml] =
        psd::html_from_text_runs(text, style_runs, paragraph_runs);
  }
  metadata[kLayerMetadataAfPendingText] = "1";
  {
    std::string box_text;
    for (int i = 0; i < 4; ++i) {
      if (i != 0) {
        box_text += ' ';
      }
      box_text += std::to_string(box[static_cast<std::size_t>(i)]);
    }
    metadata[kLayerMetadataAfTextFrame] = box_text;
  }
  if (node.type_tag == af::tag4("TxtA") && ascent > 0.0) {
    metadata[kLayerMetadataAfTextAscent] = std::to_string(ascent);
  }
  if (align > 0) {
    metadata[kLayerMetadataAfTextAlign] = std::to_string(align);
  }
  if (!transform_marker.empty()) {
    metadata[kLayerMetadataAfTextXfrm] = transform_marker;
  }

  apply_common(ctx, node, layer, display);
  if (first_alpha < 1.0F) {
    layer.set_opacity(layer.opacity() * first_alpha);
  }
  // Rough placement until the post-open pass renders real glyphs.
  layer.set_bounds(Rect{static_cast<std::int32_t>(std::lround(box[0])),
                        static_cast<std::int32_t>(std::lround(box[1])), 1, 1});
  if (transform_approximated) {
    ctx.notices.push_back("Layer '" + display +
                          "': text rotation/shear is approximated (rendered axis-aligned)");
  }
  if (unsupported_caps) {
    ctx.notices.push_back("Layer '" + display +
                          "': small/petite caps text style is not supported; rendered as typed");
  }
  if (environment_variable_is_set("PATCHY_AF_TRACE")) {
    std::fprintf(stderr, "[af] text '%s': %zu style runs, %zu paragraph runs, align %lld\n",
                 display.c_str(), style_runs.size(), paragraph_runs.size(),
                 static_cast<long long>(align));
    for (const auto& run : style_runs) {
      std::fprintf(stderr, "[af]   run %d+%d '%s' %.1f%s%s #%02x%02x%02x\n", run.start,
                   run.length, run.family.c_str(), run.size, run.bold ? " bold" : "",
                   run.italic ? " italic" : "", run.color.red, run.color.green, run.color.blue);
    }
  }
  return layer;
}

}  // namespace patchy::af::detail
