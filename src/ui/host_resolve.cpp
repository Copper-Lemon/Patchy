#include "ui/host_resolve.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#elif !defined(Q_OS_WASM) || defined(__EMSCRIPTEN_PTHREADS__)
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace patchy::ui {
namespace {

struct HostResolveState {
  std::mutex mutex;
  QObject* target{nullptr};  // the owner while it lives; null once destroyed or timed out
  bool finished{false};
};

#if !defined(Q_OS_WASM) || defined(__EMSCRIPTEN_PTHREADS__)
bool host_name_resolves(const std::string& host) {
#if defined(Q_OS_WIN)
  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    return false;
  }
#endif
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* result = nullptr;
  const int rc = getaddrinfo(host.c_str(), nullptr, &hints, &result);
  if (result != nullptr) {
    freeaddrinfo(result);
  }
#if defined(Q_OS_WIN)
  WSACleanup();
#endif
  return rc == 0;
}
#endif

}  // namespace

void resolve_host_first(QObject* owner, const QString& host, std::function<void(bool resolved)> done,
                        int timeout_ms) {
  if (owner == nullptr || !done) {
    return;
  }
  const auto ace = QUrl::toAce(host);
#if defined(Q_OS_WASM) && !defined(__EMSCRIPTEN_PTHREADS__)
  // No threads to resolve on; the browser does its own lookups.
  QTimer::singleShot(0, owner, [done = std::move(done)] { done(true); });
  (void)ace;
  (void)timeout_ms;
#else
  if (ace.isEmpty()) {
    QTimer::singleShot(0, owner, [done = std::move(done)] { done(false); });
    return;
  }
  auto state = std::make_shared<HostResolveState>();
  state->target = owner;
  // Direct (context-less) call from ~QObject: once the owner is gone the resolver thread
  // must not post to it. Anything it posted earlier dies with the owner.
  QObject::connect(owner, &QObject::destroyed, [state] {
    const std::lock_guard<std::mutex> lock(state->mutex);
    state->target = nullptr;
  });
  auto shared_done = std::make_shared<std::function<void(bool)>>(std::move(done));
  QTimer::singleShot(timeout_ms, owner, [state, shared_done] {
    {
      const std::lock_guard<std::mutex> lock(state->mutex);
      if (state->finished) {
        return;
      }
      state->finished = true;
      state->target = nullptr;
    }
    (*shared_done)(false);
  });
  std::thread([state, shared_done, name = std::string(ace.constData(), static_cast<std::size_t>(ace.size()))] {
    const bool resolved = host_name_resolves(name);
    const std::lock_guard<std::mutex> lock(state->mutex);
    if (state->finished || state->target == nullptr) {
      return;
    }
    state->finished = true;
    const QPointer<QObject> guard(state->target);
    QMetaObject::invokeMethod(
        state->target,
        [guard, shared_done, resolved] {
          if (guard != nullptr) {
            (*shared_done)(resolved);
          }
        },
        Qt::QueuedConnection);
  }).detach();
#endif
}

}  // namespace patchy::ui
