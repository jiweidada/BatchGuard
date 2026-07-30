#include "terminal_console.h"

#include <Windows.h>

#include <cstdio>
#include <iostream>

namespace batchguard::gui {

bool connectToParentTerminal() noexcept {
    bool isAttached = AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
    if (!isAttached && GetLastError() == ERROR_ACCESS_DENIED) {
        isAttached = true;
    }
    if (!isAttached) {
        return false;
    }

    FILE* errorStream = nullptr;
    if (freopen_s(&errorStream, "CONOUT$", "w", stderr) != 0) {
        return false;
    }
    SetConsoleOutputCP(CP_UTF8);
    std::cerr.clear();
    return true;
}

}
