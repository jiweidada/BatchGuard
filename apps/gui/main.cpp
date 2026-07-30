#include "main_window.h"

#include <QApplication>

namespace {

constexpr int kInternalErrorExitCode = 3;

}

int main(int argc, char* argv[]) {
    try {
        QApplication application{argc, argv};
        batchguard::gui::MainWindow mainWindow;
        mainWindow.show();
        return application.exec();
    } catch (...) {
        // 最外层只负责阻止异常越过进程边界，运行期错误由扫描控制器展示。
        return kInternalErrorExitCode;
    }
}
