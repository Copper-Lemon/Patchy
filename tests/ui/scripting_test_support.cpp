#include "scripting_test_support.hpp"
#include "core/document.hpp"
#include "ui/main_window.hpp"
#include <QFileInfo>
#include "ui/script_engine.hpp"
#include "test_harness.hpp"
#include "ui_test_support.hpp"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <span>
#include <QTreeWidgetItem>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace patchy::test::ui::scripting_support {

patchy::ui::ScriptEngineHost& start_script(patchy::ui::MainWindow& window, const QString& source) {
  auto& host = window.script_engine_host();
  patchy::ui::ScriptEngineHost::RunOptions options;
  options.name = QStringLiteral("test-script");
  (void)host.run_source(source, std::move(options));
  return host;
}

void wait_for_run_end(patchy::ui::ScriptEngineHost& host, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (host.run_active() && timer.elapsed() < timeout_ms) {
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  }
  // One extra turn so the coalesced refresh flush lands.
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
}

bool run_script(patchy::ui::MainWindow& window, const QString& source) {
  auto& host = start_script(window, source);
  wait_for_run_end(host);
  CHECK(!host.run_active());
  return !host.last_run_had_error();
}

bool backlog_contains(patchy::ui::MainWindow& window, const QString& needle) {
  for (const auto& line : window.script_engine_host().message_backlog()) {
    if (line.contains(needle)) {
      return true;
    }
  }
  return false;
}

const patchy::Layer* layer_named(const patchy::Document& document, const char* name) {
  for (const auto& layer : document.layers()) {
    if (layer.name() == name) {
      return &layer;
    }
  }
  return nullptr;
}

QTreeWidgetItem* find_child_item(QTreeWidgetItem* parent, const QString& text) {
  for (int i = 0; i < parent->childCount(); ++i) {
    if (parent->child(i)->text(0) == text) {
      return parent->child(i);
    }
  }
  return nullptr;
}

}  // namespace patchy::test::ui::scripting_support
