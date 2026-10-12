#pragma once

#include <QString>

#include <functional>

class QObject;

namespace patchy::ui {

// A getaddrinfo on a network that drops DNS queries takes 20 to 30 seconds. Qt runs
// QNetworkAccessManager's lookups on QHostInfo's thread pool, and ~QCoreApplication waits
// for that pool with no timeout, so a lookup still pending at quit froze the app after its
// window had closed (GitHub issue 48, macOS 26). Every network client therefore resolves
// its host here first, on a detached thread nothing waits for, and starts its request only
// once the resolver has answered (fast, and cached for Qt's own lookup).
inline constexpr int kHostResolveTimeoutMs = 10000;

// Calls done(resolved) on `owner`'s thread once `host` resolves or fails; a resolver still
// silent after timeout_ms reports false and is abandoned (the thread ends on its own). Never
// calls done after `owner` is destroyed.
void resolve_host_first(QObject* owner, const QString& host, std::function<void(bool resolved)> done,
                        int timeout_ms = kHostResolveTimeoutMs);

}  // namespace patchy::ui
