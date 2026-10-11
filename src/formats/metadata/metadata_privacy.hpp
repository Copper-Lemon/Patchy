#pragma once

#include "core/document_file_info.hpp"

#include <string_view>

namespace patchy::metadata {

// Why a metadata item deserves the user's attention before a file is shared
// (docs/file-info.md). The enum lives in core so DocumentFileInfo can carry it.
using PrivacyClass = MetadataPrivacy;

// Stable lowercase identifier ("location", "deviceIdentity", ...), used by the
// scripting API; never renamed.
[[nodiscard]] std::string_view privacy_class_id(PrivacyClass value) noexcept;

}  // namespace patchy::metadata
