// Regression test for GitHub issue 95 (Qt QTBUG-134784): a model reset on a list view
// that an accessibility client has read must not free table cell elements that a row
// still owns. Stock Qt 6.8.3 frees them, and releasing the row later crashes in
// -[QMacAccessibilityElement dealloc]; the cocoa plugin built by
// scripts/setup-qt-cocoa-patch-mac.sh keeps them alive until the row lets go.
//
// Runs on the real cocoa platform (a logged-in GUI session), not offscreen:
//   patchy_mac_a11y_tests <dir holding platforms/libqcocoa.dylib>   the plugin to test
//   patchy_mac_a11y_tests --stock                                   the Qt kit's own plugin
// With no argument, or a folder without the plugin, it reports a skip and exits 0.
// Exit codes: 0 pass or skip, 1 failure.

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGui/qaccessible.h>

#import <AppKit/AppKit.h>
#include <dlfcn.h>
#include <objc/runtime.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

namespace {

bool g_cell_deallocated = false;
char g_sentinel_key = 0;

void fail(const char *message) {
  std::fprintf(stderr, "[FAIL] cocoa_a11y_cell_lifetime: %s\n", message);
  std::fflush(stderr);
  // _exit, not exit: after a failure the autorelease pools still hold the dangling
  // cell, and draining them would crash instead of reporting.
  _exit(1);
}

}  // namespace

// Rides on the cell element as an associated object, so its dealloc reports the cell's.
@interface PatchyCellDeallocSentinel : NSObject
@end

@implementation PatchyCellDeallocSentinel
- (void)dealloc {
  g_cell_deallocated = true;
  [super dealloc];
}
@end

namespace {

// Depth-first search for the accessibility element that represents a table (has rows).
id find_table_element(id element, int depth) {
  if (!element || depth > 12) {
    return nil;
  }
  if ([element respondsToSelector:@selector(accessibilityRows)]) {
    NSArray *rows = [element accessibilityRows];
    if (rows.count > 0) {
      return element;
    }
  }
  NSArray *children = [element respondsToSelector:@selector(accessibilityChildren)]
                          ? [element accessibilityChildren]
                          : nil;
  for (id child in children) {
    if (id found = find_table_element(child, depth + 1)) {
      return found;
    }
  }
  return nil;
}

QString loaded_cocoa_plugin_path() {
  Class element_class = NSClassFromString(@"QMacAccessibilityElement");
  if (!element_class) {
    return {};
  }
  Dl_info info{};
  IMP imp = class_getMethodImplementation(element_class, @selector(accessibilityRole));
  if (!imp || dladdr(reinterpret_cast<const void *>(imp), &info) == 0 || !info.dli_fname) {
    return {};
  }
  return QFileInfo(QString::fromUtf8(info.dli_fname)).canonicalFilePath();
}

}  // namespace

int main(int argc, char **argv) {
  const QString arg = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
  const bool stock = arg == QLatin1String("--stock");
  QString expected_plugin;
  if (!stock) {
    const QString plugin = QDir(arg).filePath(QStringLiteral("platforms/libqcocoa.dylib"));
    if (arg.isEmpty() || !QFileInfo::exists(plugin)) {
      std::printf("[SKIP] cocoa_a11y_cell_lifetime: no patched cocoa plugin given (run "
                  "scripts/setup-qt-cocoa-patch-mac.sh and pass its folder)\n");
      return 0;
    }
    expected_plugin = QFileInfo(plugin).canonicalFilePath();
    // The only plugin folder, so the kit's own cocoa plugin cannot win the "cocoa" key
    // (QT_QPA_PLATFORM_PLUGIN_PATH only appends a folder in Qt 6.8).
    QCoreApplication::setLibraryPaths({QDir(arg).absolutePath()});
  }
  qputenv("QT_QPA_PLATFORM", "cocoa");

  QApplication app(argc, argv);

  const QString loaded_plugin = loaded_cocoa_plugin_path();
  std::fprintf(stderr, "cocoa plugin: %s\n", qPrintable(loaded_plugin));
  if (!stock && loaded_plugin != expected_plugin) {
    std::fprintf(stderr, "expected: %s\n", qPrintable(expected_plugin));
    fail("the cocoa platform did not load the plugin under test");
  }

  // A list with item widgets on its rows, like the Layers panel.
  QWidget window;
  window.setWindowTitle(QStringLiteral("Patchy accessibility cell test"));
  auto *layout = new QVBoxLayout(&window);
  auto *list = new QListWidget(&window);
  layout->addWidget(list);
  for (int i = 0; i < 3; ++i) {
    auto *item = new QListWidgetItem(QStringLiteral("Layer %1").arg(i), list);
    list->setItemWidget(item, new QLabel(QStringLiteral("Layer row %1").arg(i)));
  }
  window.resize(240, 160);
  window.move(0, 0);
  window.show();
  QApplication::processEvents();

  id table = nil;
  id cell = nil;
  @autoreleasepool {
    NSView *view = reinterpret_cast<NSView *>(window.winId());
    // The children query activates Qt's accessibility, like a real client's first request.
    // QNSView answers only the legacy attribute API, so it is called by selector (the
    // direct call is deprecated).
    NSArray *top = [view performSelector:@selector(accessibilityAttributeValue:)
                              withObject:NSAccessibilityChildrenAttribute];
    if (!QAccessible::isActive()) {
      fail("querying the window did not activate Qt accessibility");
    }
    for (id element in top) {
      if ((table = find_table_element(element, 0))) {
        break;
      }
    }
    if (!table) {
      fail("no table element found for the list");
    }
    [table retain];

    NSArray *rows = [table accessibilityRows];
    NSArray *cells = [rows[0] accessibilityChildren];  // creates placeholder cells
    if (cells.count == 0) {
      fail("the first row has no cell elements");
    }
    cell = cells[0];
    // Reading the title resolves the placeholder into the accessibility cache.
    NSString *title = [cell accessibilityTitle];
    if (![title isEqualToString:@"Layer 0"]) {
      std::fprintf(stderr, "cell title: %s\n", title ? title.UTF8String : "(nil)");
      fail("the cell element did not resolve to the first list item");
    }
    PatchyCellDeallocSentinel *sentinel = [[PatchyCellDeallocSentinel alloc] init];
    objc_setAssociatedObject(cell, &g_sentinel_key, sentinel, OBJC_ASSOCIATION_RETAIN);
    [sentinel release];
  }
  // The query's autoreleased arrays are gone; the row's columns array (and, with the
  // fix, the cache) is all that keeps the cell alive.

  @autoreleasepool {
    // What every Layers panel refresh does: a model reset deletes the cell interfaces,
    // and the cocoa plugin's model-change handler autoreleases the table's old rows.
    list->clear();
    if (g_cell_deallocated) {
      fail("the model reset freed a cell element its row still holds (QTBUG-134784)");
    }
    [table accessibilityRows];
  }
  // Draining the pool released the old rows, and with them their cells.
  if (!g_cell_deallocated) {
    fail("the cell element was never released after its row went away");
  }
  [table release];

  std::printf("[PASS] cocoa_a11y_cell_lifetime\n");
  return 0;
}
