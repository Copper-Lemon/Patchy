#pragma once

// A connector (patchy-mcp) can outlive the Patchy it came from: an update or a rebuild sets
// the running executable aside and puts a new one at its path while AI apps keep their
// connection open (docs/auto-update.md). The connector then says so in every tool result,
// so the assistant can tell the user to restart the app instead of working on stale code.

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class QJsonArray;
class QJsonObject;

namespace patchy::ui {

// Remembers the file at `path` and reports when a different file has taken its place.
class ReplacedFileWatch {
public:
  explicit ReplacedFileWatch(QString path);
  // True once the file is gone or its size or modification time differs. Never true for a
  // path that had no file to begin with.
  [[nodiscard]] bool replaced() const;

private:
  QString path_;
  bool existed_{false};
  qint64 size_{0};
  QDateTime modified_;
};

// Starts watching this process's executable; the first call is the reference point, so
// connector startup calls it before serving anything.
void watch_running_executable();
// English, for the assistant (MCP wire text is not translated). Empty while the running
// executable is still the installed one.
[[nodiscard]] QString connector_update_notice();
// Adds "updateNotice" to a tool result's structured data and a text item saying the same,
// when there is a notice. Returns whether it changed anything.
bool add_connector_update_notice(QJsonObject& structured_content, QJsonArray& content);


// Someone who only ever uses Patchy through an AI app never sees the app's update prompt,
// so an owned connector runs the same update check and get_info carries its answer as
// "updateAvailable". English wire text; empty until a newer version is known.
[[nodiscard]] QString connector_update_available_message(const QString& new_version, const QString& download_url);
void set_connector_update_available(const QString& message);
[[nodiscard]] QString connector_update_available();

}  // namespace patchy::ui
