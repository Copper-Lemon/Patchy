#pragma once

#include <QStandardPaths>
#include <QString>

class QTreeWidgetItem;
namespace patchy { class Document; class Layer; }
namespace patchy::ui { class MainWindow; class ScriptEngineHost; }

namespace patchy::test::ui::scripting_support {

patchy::ui::ScriptEngineHost& start_script(patchy::ui::MainWindow& window, const QString& source);

void wait_for_run_end(patchy::ui::ScriptEngineHost& host, int timeout_ms = 15000);

// Runs to completion; CHECKs that the run ended and reports whether it ended clean.
bool run_script(patchy::ui::MainWindow& window, const QString& source);

bool backlog_contains(patchy::ui::MainWindow& window, const QString& needle);

const patchy::Layer* layer_named(const patchy::Document& document, const char* name);

// Redirects QStandardPaths writable locations (the user scripts folder) into
// the per-user qttest sandbox so shadow-override saves never touch the real
// profile; exception-safe so a failing CHECK cannot leave test mode on.
struct StandardPathsTestMode {
  StandardPathsTestMode() { QStandardPaths::setTestModeEnabled(true); }
  ~StandardPathsTestMode() { QStandardPaths::setTestModeEnabled(false); }
};

QTreeWidgetItem* find_child_item(QTreeWidgetItem* parent, const QString& text);

}  // namespace patchy::test::ui::scripting_support
