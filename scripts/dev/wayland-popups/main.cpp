// Popup placement probe for docs/wayland.md (GitHub issue 100). A window with Image > Mode,
// a combo box and Patchy's popup anchor workaround (src/ui/wayland_popup_anchor.cpp, on
// when PROBE_FIX is set). run.sh clicks the menu bar item or the combo with a virtual
// pointer; the program then opens the submenu with a Right key event, prints the popups'
// logical and native geometry, and quits. WAYLAND_DEBUG=1 shows what the compositor got.
#include "ui/wayland_popup_anchor.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>
#include <QWindow>
#include <qpa/qplatformwindow.h>

#include <cstdio>

static void dump(const char* name, QWidget* widget) {
  const QRect logical = widget->geometry();
  QRect native;
  if (widget->windowHandle() != nullptr && widget->windowHandle()->handle() != nullptr) {
    native = widget->windowHandle()->handle()->geometry();
  }
  std::printf("%s logical=(%d,%d %dx%d) native=(%d,%d %dx%d) visible=%d\n", name, logical.x(), logical.y(),
              logical.width(), logical.height(), native.x(), native.y(), native.width(), native.height(),
              int(widget->isVisible()));
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  if (qEnvironmentVariableIsSet("PROBE_FIX")) {
    patchy::ui::install_wayland_popup_anchor_fix();
  }
  QMainWindow window;
  window.menuBar()->addMenu(QStringLiteral("File"))->addAction(QStringLiteral("New"));
  QMenu* image = window.menuBar()->addMenu(QStringLiteral("Image"));
  QMenu* mode = image->addMenu(QStringLiteral("Mode"));
  mode->addAction(QStringLiteral("Bitmap"));
  mode->addAction(QStringLiteral("Grayscale"));
  mode->addAction(QStringLiteral("RGB Color"));
  image->addAction(QStringLiteral("Adjustments"));
  window.setCentralWidget(new QWidget(&window));
  auto* combo = new QComboBox(window.centralWidget());
  combo->addItems({QStringLiteral("RGB Color"), QStringLiteral("Grayscale"), QStringLiteral("Bitmap")});
  combo->setGeometry(20, 40, 160, 24);
  window.resize(800, 600);
  window.show();

  QTimer::singleShot(500, [&] {
    const QRect item = window.menuBar()->actionGeometry(image->menuAction());
    const QRect box(combo->mapTo(&window, QPoint(0, 0)), combo->size());
    std::printf("click targets (logical): Image item (%d,%d %dx%d), combo (%d,%d %dx%d)\n", item.x(),
                item.y(), item.width(), item.height(), box.x(), box.y(), box.width(), box.height());
    std::fflush(stdout);
  });
  QTimer::singleShot(2500, [&] {
    if (image->isVisible()) {
      image->setActiveAction(mode->menuAction());
      QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
      QCoreApplication::sendEvent(image, &right);
    }
  });
  QTimer::singleShot(4000, [&] {
    std::printf("QT_SCALE_FACTOR=%s qt=%s platform=%s\n", qgetenv("QT_SCALE_FACTOR").constData(), qVersion(),
                qPrintable(QGuiApplication::platformName()));
    dump("window", &window);
    dump("image", image);
    dump("mode", mode);
    dump("combo-list", combo->view()->window());
    std::fflush(stdout);
  });
  QTimer::singleShot(7000, &app, &QApplication::quit);
  return app.exec();
}
