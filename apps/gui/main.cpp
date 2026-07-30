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
        // GUI 尚未建立统一错误对话框，阶段 10 先保证异常不会越过进程边界。
        return kInternalErrorExitCode;
    }
}
