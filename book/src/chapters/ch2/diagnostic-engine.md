# Художник консоли

## Введение

Мы создали координаты, научились безопасно хранить файлы в памяти и построили удобный builder для ошибок.
Остался финальный шаг этой главы — написать движок рендеринга (**`DiagnosticEngine`**), который превратит
сырые объекты диагностик в красивые, цветные и аккуратно выровненные текстовые сообщения в терминале.

`DiagnosticEngine` создаётся в единственном экземпляре на весь процесс компиляции. Он принимает по ссылке
`SourceMgr`, аккумулирует сгенерированные ошибки и предоставляет метод `Render()` для вывода их в `std::cerr`.

## Цветной вывод и ANSI-коды

Чтобы диагностика легко читалась, ключевые элементы (ошибки, предупреждения, подчёркивания) должны выделяться
цветом. Для этого используются **ANSI escape-последовательности** — специальные символьные управляющие коды,
которые терминал интерпретирует как инструкции по изменению стиля текста.

На Unix-подобных системах (Linux, macOS) ANSI-коды работают из коробки. Однако на Windows для стандартного
потока вывода `STD_OUTPUT_HANDLE` нужно явно включить режим `ENABLE_VIRTUAL_TERMINAL_PROCESSING`.

```cpp
// include/pebble/diagnostic/colors.h

#pragma once

namespace pebble {

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

}
```

Каждая константа — это служебная последовательность байт. Например, `\033[31m` переключает цвет текста
на красный, а `\033[0m` (`RESET`) сбрасывает все стили обратно в стандартные, чтобы не "покрасить" весь
последующий вывод консоли.

Пример использования:

```cpp
// src/main.cpp

#include <iostream>
#include "pebble/diagnostic/colors.h"

using namespace pebble;

int
main () {
    EnableVirtualTerminalProcessing ();

    // Вывод строки "Hello world!" жирным красным цветом
    std::cout << color::BOLD << color::RED << "Hello world!" << color::RESET << '\n';
    return 0;
}
```

## Проблема выравнивания номеров строк

Взгляните на то, как выглядит вывод ошибок из реальных исходных файлов. Номера строк могут быть однозначными (строка `2`), двузначными (строка `45`) или четырёхзначными (строка `1024`):

```text
 2 | var x = 10;
   |     ^
...
45 | var y = 20;
   |     ^
```

Если разделительную вертикальную черту `|` выводить без учёта ширины самого большого номера строки в ошибке,
верстка моментально «поплывёт».

Чтобы вертикальная колонка разделителей всегда оставалась идеальной прямой связывающей линией, нам нужно
знать количество цифр в номере строки. Для этого реализуем вспомогательную функцию `DigitCount`, использующую
десятичный логарифм `std::log10`:

```cpp
inline int
DigitCount (std::uint32_t line) {
    if (line == 0) {
        return 1;
    }
    return static_cast<int> (std::log10 (line)) + 1;
}
```

Математически логарифм `std::log10(100)` равен `2`, прибавляя `1`, мы получаем `3` (количество цифр в числе
100). Это дает мгновенный результат за `O(1)` без преобразования числа в строку.

## Движок диагностики

```cpp
// include/pebble/diagnostic/engine.h

#pragma once
#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/annotation.h"
#include "pebble/diagnostic/builder.h"
#include "pebble/diagnostic/colors.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace pebble::diagnostic {

inline int
DigitCount (std::uint32_t line) {
    if (line == 0) {
        return 1;
    }
    return static_cast<int> (std::log10 (line)) + 1;
}

class DiagnosticEngine {
    basic::SourceMgr              &_mgr;
    std::vector<DiagnosticBuilder> _builders;
    bool                           _hasErrs{};

public:
    explicit DiagnosticEngine (basic::SourceMgr &mgr) : _mgr (mgr) {}

    DiagnosticBuilder &
    Report (DiagCode code, const std::string &msg, DiagSeverity severity) {
        _builders.emplace_back (code, msg, severity);
        if (severity == DiagSeverity::Error) {
            _hasErrs = true;
        }
        return _builders.back ();
    }

    bool
    HasErrors () const {
        return _hasErrs;
    }

    void
    Render () {
        int i = 0;
        for (DiagnosticBuilder &diag : _builders) {
            if (i != 0) {
                std::cerr << '\n';
            }
            renderDiag (diag);
            ++i;
        }
    }

    std::vector<DiagnosticBuilder> &
    Builders () {
        return _builders;
    }

    void
    SortDiagSpans () {
        for (auto &diag : _builders) {
            sortDiagSpans (diag);
        }
    }

    basic::SourceMgr &
    SourceMgr () {
        return _mgr;
    }

private:
    void
    sortDiagSpans (DiagnosticBuilder &diag);

    void
    renderDiag (DiagnosticBuilder &diag) {
        sortDiagSpans (diag);
        printDiagnosticHeader (diag);
        printDiagnosticBody (diag);
    }

    static void
    printDiagnosticHeader (DiagnosticBuilder &diag);

    void
    printDiagnosticBody (DiagnosticBuilder &diag);

    void
    printAnnotation (const Annotation &annotation, std::uint32_t maxLineWidth);

    static void
    printHelp (const Help &help, std::uint32_t maxLineWidth);

    static void
    printNote (const Note &note, std::uint32_t maxLineWidth);
};

}
```

## Реализация форматирования и рендеринга

Перейдём к файлу реализации `engine.cpp`. Здесь происходят все основные операции по преобразованию
внутренних структур в текст.

```cpp
// src/lib/diagnostic/engine.cpp

#include "pebble/diagnostic/engine.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/diagnostic/colors.h"
#include <format>

namespace pebble::diagnostic {

#define variant(kind, val)                                                               \
    case DiagSeverity::kind:                                                             \
        return val;

static Color
SeverityColor (DiagSeverity severity) {
    switch (severity) {
        variant (Warning, color::YELLOW);
        variant (Error, color::RED);
        variant (Note, color::CYAN);
    }
    return color::RESET;
}

static const char *
SeverityName (DiagSeverity severity) {
    switch (severity) {
        variant (Warning, "warning");
        variant (Error, "error");
        variant (Note, "note");
    }
    return "<unknown>";
}

#undef variant

char
SeverityPrefix (DiagSeverity severity) {
    return (char) toupper (SeverityName (severity)[0]);
}

int
DiagCodeToIntegerCode (DiagCode code) {
    if (code <= ERR_CODE_LAST) {
        return static_cast<int> (code);
    }
    if (code <= WARN_CODE_LAST) {
        return static_cast<int> (
            static_cast<std::uint8_t> (code)
            - static_cast<std::uint8_t> (WARN_CODE_START));
    }
    return 0;
}

void
DiagnosticEngine::sortDiagSpans (DiagnosticBuilder &diag) {
    std::ranges::sort (
        diag.Annotations (),
        [&] (const Annotation &a, const Annotation &b) {
            return _mgr.FindLoc (a.Span.Start).Line < _mgr.FindLoc (b.Span.Start).Line;
        });
}

void
DiagnosticEngine::printDiagnosticHeader (DiagnosticBuilder &diag) {
    std::cerr << color::BOLD << SeverityColor (diag.Severity ());

    std::cerr << SeverityName (diag.Severity ()) << color::RESET << '[';
    std::cerr << color::BOLD << SeverityColor (diag.Severity ());
    std::string errCode = std::format ("{:04}", DiagCodeToIntegerCode (diag.Code ()));
    std::cerr << SeverityPrefix (diag.Severity ()) << errCode << color::RESET
              << "]: " << diag.Msg () << '\n';
}

void
DiagnosticEngine::printDiagnosticBody (DiagnosticBuilder &diag) {
    auto maxAnnotation = std::max_element (
        diag.Annotations ().begin (),
        diag.Annotations ().end (),
        [&] (const Annotation &a, const Annotation &b) {
            return _mgr.FindLoc (a.Span.Start).Line
                   < _mgr.FindLoc (b.Span.Start).Line;
        });
    auto loc          = _mgr.FindLoc (maxAnnotation->Span.Start);
    auto maxLine      = loc.Line;
    auto maxLineWidth = DigitCount (maxLine);
    std::cerr << color::RESET << std::string (maxLineWidth, ' ') << "--> "
              << _mgr.GetFile (maxAnnotation->Span.Start.FileId).Name << ':'
              << loc.Line << ':' << loc.Col << '\n';
    for (const auto &annotation : diag.Annotations ()) {
        if (annotation == *diag.Annotations ().begin ()) {
            std::cerr << color::RESET;
            std::cerr << std::string (maxLineWidth, ' ') << " |\n";
        }
        printAnnotation (annotation, maxLineWidth);
        std::cerr << std::string (maxLineWidth, ' ') << " |\n";
    }
    for (const auto &help : diag.Helps ()) {
        printHelp (help, maxLineWidth);
    }
    for (const auto &note : diag.Notes ()) {
        printNote (note, maxLineWidth);
    }
}

void
DiagnosticEngine::printAnnotation (
    const Annotation &annotation, std::uint32_t maxLineWidth) {
    auto startLoc = _mgr.FindLoc (annotation.Span.Start);
    auto endLoc   = _mgr.FindLoc (annotation.Span.End);

    std::cerr << color::YELLOW << std::format ("{:{}}", startLoc.Line, maxLineWidth)
              << color::RESET << " | ";
    auto lineContent = _mgr.GetLineContent (annotation.Span.Start.FileId, startLoc.Line);
    std::cerr << lineContent << '\n';

    std::cerr << std::string (maxLineWidth, ' ') << " | ";
    std::cerr << std::string (startLoc.Col - 1, ' ');
    char highlighter = annotation.IsPrimary ? '^' : '-';
    char highlighter    = annotation.IsPrimary ? '^' : '-';
    auto highlighterLen = std::min (
        annotation.Span.End.Start - annotation.Span.Start.Start,
        static_cast<std::uint32_t> (lineContent.size ()) - startLoc.Col + 1);
    std::cerr << color::RED << std::string (highlighterLen, highlighter);
    std::cerr << color::RESET << ' ' << annotation.Label << '\n';
}

void
DiagnosticEngine::printHelp (const Help &help, std::uint32_t maxLineWidth) {
    std::cerr << color::RESET << std::string (maxLineWidth, ' ') << " = ";
    std::cerr << color::CYAN << "help: " << color::RESET << help.Msg << '\n';
}

void
DiagnosticEngine::printNote (const Note &note, std::uint32_t maxLineWidth) {
    std::cerr << color::RESET << std::string (maxLineWidth, ' ') << " = ";
    std::cerr << color::CYAN << "note: " << color::RESET << note.Msg << '\n';
}

};
```

### Как работают внутренние алгоритмы?

Разберём каждую ключевую функцию и её роль:

#### Формирование шапки (`printDiagnosticHeader`)

Шапка выводит уровни важности и числовой код ошибки.

* Функция `DiagCodeToIntegerCode` приводит enum `DiagCode` к порядковому числу. Для ошибок это просто
целочисленное значение enum, а для предупреждений вычитается смещение `WARN_CODE_START`, чтобы отсчёт
предупреждений тоже начинался с `0`.
* С помощью `std::format("{:04}", ...)` число форматируется в строку из 4 цифр с ведущими нулями
(например, `0012`).
* Символ `SeverityPrefix` берет первую букву от уровня важности и переводит её в верхний регистр
(`E` для `error`, `W` для `warning`).

В результате выводится идеальная строка вида:

```text
error[E0012]: variable 'x' is already defined
```

#### Сортировка спанов (`sortDiagSpans`)

В процессе работы парсера аннотации могут добавляться в произвольном порядке. Метод `sortDiagSpans`
использует `std::ranges::sort` с лямбда-функцией сравнения по номерам строк. Это гарантирует, что в консоли
вырезки кода всегда будут идти по порядку сверху вниз (от меньших номеров строк к большим).

#### Вычисление отступов и отрисовка (`printDiagnosticBody` и `printAnnotation`)

1. `printDiagnosticBody` сначала находит аннотацию с максимальным номером строки с помощью `std::max_element`.
2. По найденной максимальной строке вычисляется значение `maxLineWidth` через `DigitCount`.
3. Печатается заголовок файла `--> src/main.pebble:3:9` с динамическим отступом из пробелов слева, равным `maxLineWidth`.
4. Далее поочередно вызывается `printAnnotation`:
   * С помощью `std::format("{:{}}", startLoc.Line, maxLineWidth)` выводится номер строки, выравниваемый по правому краю ширины `maxLineWidth`.
   * Из `SourceMgr` по номеру строки запрашивается сам текст исходного кода `lineContent`.
   * На следующей строке выводятся пробелы до колонки `startLoc.Col - 1`.
   * Выбирается символ подчёркивания (`^` если `annotation.IsPrimary == true`, иначе `-`).
   * Печатается цепочка символов подчёркивания длиной минимального значения между
   `annotation.Span.End.Start - annotation.Span.Start.Start` и расстоянием от начала подчеркивания до конца
   строки (для того, чтобы подчеркивание случайно не вышло за пределы строки) и метка `annotation.Label`.

#### Вывод примечаний и подсказок (`printNote` и `printHelp`)

После того как все строки кода с аннотациями отрисованы, `printDiagnosticBody` проходит по вектору подсказок (`diag.Helps()`) и примечаний (`diag.Notes()`).

Для каждого объекта `Help` вызывается `printHelp`:

1. Печатается отступ из `maxLineWidth` пробелов, чтобы знак равенства `=` встал ровно под колонкой, где раньше выводились разделители `|`.
2. Выводится знак `=` с пробелами.
3. Голубым цветом (`color::CYAN`) печатается префикс `help:`.
4. После префикса сбрасывается цвет и выводится сам текст примечания `help.Msg`.

Для каждого объекта `Note` вызывается `printNote`:

1. Печатается отступ из `maxLineWidth` пробелов, чтобы знак равенства `=` встал ровно под колонкой,
где раньше выводились разделители `|`.
2. Выводится знак `=` с пробелами.
3. Голубым цветом (`color::CYAN`) печатается префикс `note:`.
4. После префикса сбрасывается цвет и выводится сам текст примечания `note.Msg`.

Благодаря этому примечания выглядят как аккуратные сноски внизу блока ошибки, не привязанные к конкретной
строке кода, но идеально вписывающиеся в общую колонку выравнивания.

## Полный пример работы

Продемонстрируем работу системы диагностики в действии:

```cpp
// src/main.cpp

#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/engine.h"

using namespace pebble;
using namespace pebble::basic;
using namespace pebble::diagnostic;

int
main () {
    EnableVirtualTerminalProcessing ();

    SourceMgr srcMgr;
    uint32_t fileId = srcMgr.AddFile (
        "src/main.pebble",
        "fn main(): int32 {\n"
        "    var x = 10;\n"
        "    var x = 20;\n"
        "    return 0;\n"
        "}\n"
    );

    DiagnosticEngine engine (srcMgr);

    // Предположим, семантика обнаружила дублирование переменной 'x'
    Pos firstDeclStart{ 27, fileId }; // var x (строка 2)
    Pos firstDeclEnd{ 28, fileId };

    Pos secondDeclStart{ 43, fileId }; // var x (строка 3)
    Pos secondDeclEnd{ 44, fileId };

    engine.Report (DiagCode::ERedefinition, "variable 'x' is already defined", DiagSeverity::Error)
        .AddAnnotation (Span{ firstDeclStart, firstDeclEnd }, "previous definition was here", false)
        .AddAnnotation (Span{ secondDeclStart, secondDeclEnd }, "redefined here", true)
        .AddNote ("identifiers within the same scope must be unique");

    // Рендерим накопленные ошибки
    engine.Render ();

    return 0;
}
```

Запустив этот код, вы получите следующий сияющий цветной вывод прямо в консоли:

```text
error[E0007]: variable 'x' is already defined
 --> src/main.pebble:3:9
  |
2 |     var x = 10;
  |         - previous definition was here
  |
3 |     var x = 20;
  |         ^ redefined here
  |
  = note: identifiers within the same scope must be unique
```
