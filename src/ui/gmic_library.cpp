#include "ui/gmic_library.hpp"

#include "filters/gmic/gmic_engine.hpp"

#include <QByteArray>
#include <QFile>
#include <QString>

// Q_INIT_RESOURCE must run from outside any namespace. The library text lives in
// gmic.qrc, compiled into the static patchy_ui only in builds with the engine;
// naming it here keeps the linker from dropping it.
static void ensure_gmic_resources_registered() {
#if defined(PATCHY_HAVE_GMIC)
  static const bool registered = [] {
    Q_INIT_RESOURCE(gmic);
    return true;
  }();
  (void)registered;
#endif
}

namespace patchy::ui {

namespace {

[[nodiscard]] std::string read_resource(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QByteArray bytes = file.readAll();
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

[[nodiscard]] GmicLibrary load_library() {
  GmicLibrary library;
  if (!gmic::engine_available()) {
    return library;
  }
  ensure_gmic_resources_registered();
  library.commands = read_resource(QStringLiteral(":/gmic/gmic_stdlib.gmic"));
  library.catalog = gmic::parse_catalog(library.commands);
  gmic::retain_allowed(library.catalog,
                       gmic::parse_allowlist(read_resource(QStringLiteral(":/gmic/patchy_filters_allowlist.txt"))));
  return library;
}

}  // namespace

const GmicLibrary& gmic_library() {
  static const GmicLibrary library = load_library();
  return library;
}

bool gmic_filters_available() {
  return gmic::engine_available() && !gmic_library().catalog.filters.empty();
}

}  // namespace patchy::ui
