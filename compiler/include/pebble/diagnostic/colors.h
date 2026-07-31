#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

inline void
EnableVirtualTerminalProcessing () {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle (STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode (hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode (hOut, dwMode);
        }
    }
#endif
}

using Color = const char *;

namespace color {

constexpr Color RESET  = "\033[0m";
constexpr Color BOLD   = "\033[1m";
constexpr Color RED    = "\033[31m";
constexpr Color YELLOW = "\033[33m";
constexpr Color BLUE   = "\033[34m";
constexpr Color CYAN   = "\033[36m";

}
