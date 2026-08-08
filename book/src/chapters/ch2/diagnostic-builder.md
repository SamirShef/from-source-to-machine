# Мастер диагнозов

## Введение

Мы уже заложили крепкий фундамент: научились работать с координатами (`Pos`, `Span`) и хранить файлы в
`SourceMgr`. Теперь пора ответить на вопрос: **как сам компилятор формирует диагностическое сообщение
перед тем, как выдать его пользователю?**

В реальном компиляторе фазы анализа (лексер, парсер, проверщик типов) не должны напрямую заниматься
форматированием текста и раскрашиванием консоли. Их задача — лишь зафиксировать факт проблемы и перечислить
связанные с ней участки кода.

Для этого используется паттерн **Строитель** (`DiagnosticBuilder`). Он аккумулирует данные об ошибке,
а отдельный модуль-рендер позже превращает этот набор данных в красивый текстовый отчёт или JSON для IDE.

## Анатомия диагностики и аннотации

Каждое диагностическое сообщение состоит из следующих частей:

1. **Серьёзность (`DiagSeverity`):** Ошибка, предупреждение или примечание.
2. **Код ошибки (`DiagCode`):** Уникальный идентификатор (например, `E0012` или `W0001`).
3. **Заголовок:** Краткое описание проблемы.
4. **Список аннотаций (`Annotation`):** Подсветки участков кода с текстовыми подписями.
5. **Список подсказок (`Help`) и примечаний (`Note`):** Дополнительные текстовые пояснения.

### Первичные и вторичные аннотации (`IsPrimary`)

Аннотация — это не просто ссылка на `Span`. Это указатель, определяющий, *как* пользователь
воспринимает ошибку.

В структуре `Annotation` есть флаг `IsPrimary`:

* **`IsPrimary = true` (Первичная):** Указывает на место, где ошибка **спровоцирована** прямо сейчас. Подчёркивается символом `^`.
* **`IsPrimary = false` (Вторичная):** Указывает на сопутствующий контекст, который **привёл** к ошибке.
Подчёркивается символом `-`.

```cpp
// include/pebble/diagnostic/annotation.h

$#pragma once
$#include "pebble/diagnostic/span.h"
$#include <string>
$#include <utility>
$
$namespace pebble::diagnostic {
$
struct Annotation {
    struct Span Span;
    std::string Label;
    bool        IsPrimary = true;

    Annotation (struct Span span, std::string label, bool isPrimary = true)
        : Span (span), Label (std::move (label)), IsPrimary (isPrimary) {}

    auto operator<=> (const Annotation &) const = default;
};
$
$}
```

Посмотрите, как это разделение преображает восприятие повторного объявления переменной:

```pebble
fn main(): int32 {
    var x = 10;
    var x = 20;
    return 0;
}
```

```text
error[E0012]: variable 'x' is already defined
 --> src/main.pebble:3:9
  |
2 |     var x = 10;
  |         - previous definition was here
3 |     var x = 20;
  |         ^ redefined here
  |
```

Различие в символах (`-` для первого объявления и `^` для повторного) мгновенно направляет взгляд программиста на источник проблемы, сохраняя при этом важный контекст.

## Примечания (`Note`), подсказки (`Help`) и коды ошибок

Иногда ошибки требуют текстовых подсказок или пояснений без привязки к конкретному диапазону кода.
Для этого служат структуры `Help` `Note`:

```cpp
// include/pebble/diagnostic/help.h

$#pragma once
$#include <string>
$
$namespace pebble::diagnostic {
$
struct Help {
    std::string Msg;
};
$
$}
```

```cpp
// include/pebble/diagnostic/note.h

$#pragma once
$#include <string>
$
$namespace pebble::diagnostic {
$
struct Note {
    std::string Msg;
};
$
$}
```

Каждой диагностике также присваивается строго определённый код из перечисления `DiagCode`:

```cpp
// include/pebble/diagnostic/codes.h

$#pragma once
$#include <cstdint>
$
$namespace pebble::diagnostic {
$
enum class DiagSeverity : std::uint8_t { Warning, Error, Note };

enum class DiagCode : std::uint8_t {
    // Ошибки (Errors)
    EUnexpectedToken,
    EExpectedExpr,
    EUnclosedStrLit,
    EUnclosedCharLit,
    EIncorrectCharLitLen,
    EIntSuffixForFloat,
    ERedefinition,

    // Предупреждения (Warnings)
    WUnusedVar,
    WLossPrecision,
};

constexpr inline DiagCode ERR_CODE_START = DiagCode::EUnexpectedToken;
constexpr inline DiagCode ERR_CODE_LAST
    = static_cast<DiagCode> (static_cast<std::uint8_t> (DiagCode::WUnusedVar) - 1);

constexpr inline DiagCode WARN_CODE_START = DiagCode::WUnusedVar;
constexpr inline DiagCode WARN_CODE_LAST  = DiagCode::WLossPrecision;
$
$}
```

## Конструирование через Fluent API: `DiagnosticBuilder`

Класс `DiagnosticBuilder` предоставляет удобный интерфейс для пошаговой сборки диагностики.
Он использует паттерн **Fluent API** (цепочка вызовов), где методы добавления возвращают ссылку на сам
объект (`*this`).

```cpp
// include/pebble/diagnostic/codes.h

$#pragma once
$#include "pebble/diagnostic/annotation.h"
$#include "pebble/diagnostic/codes.h"
$#include "pebble/diagnostic/note.h"
$#include "pebble/diagnostic/help.h"
$#include "pebble/diagnostic/span.h"
$#include <string>
$#include <utility>
$#include <vector>
$
$namespace pebble::diagnostic {
$
class DiagnosticBuilder {
    DiagCode                _code;
    std::string             _msg;
    DiagSeverity            _severity;
    std::vector<Annotation> _annotations;
    std::vector<Note>       _notes;
    std::vector<Help>       _helps;

public:
    DiagnosticBuilder (DiagCode code, std::string msg, DiagSeverity severity)
        : _code (code),_msg (std::move (msg)), _severity (severity) {}

    DiagnosticBuilder &
    AddAnnotation (Span span, std::string label = "", bool isPrimary = true) {
        _annotations.emplace_back (span, std::move (label), isPrimary);
        return *this;
    }

    DiagnosticBuilder &
    AddAnnotation (
        basic::Pos start, basic::Pos end, std::string label = "", bool isPrimary = true) {
        return AddAnnotation (Span (start, end), std::move (label), isPrimary);
    }

    DiagnosticBuilder &
    AddHelp (std::string text) {
        _helps.emplace_back (Help{ std::move (text) });
        return *this;
    }

    DiagnosticBuilder &
    AddNote (std::string text) {
        _notes.emplace_back (Note{ std::move (text) });
        return *this;
    }

    DiagCode
    Code () const {
        return _code;
    }

    const std::string &
    Msg () const {
        return _msg;
    }

    DiagSeverity
    Severity () const {
        return _severity;
    }

    std::vector<Annotation> &
    Annotations () {
        return _annotations;
    }

    const std::vector<Annotation> &
    Annotations () const {
        return _annotations;
    }

    const std::vector<Help> &
    Helps () const {
        return _helps;
    }

    const std::vector<Note> &
    Notes () const {
        return _notes;
    }
};
$
$}
```

### Как это выглядит на практике?

Теперь в любом месте компилятора (например, при проверке имён в семантическом анализаторе) создание
комплексной ошибки с несколькими аннотациями выглядит элегантно и читаемо:

```cpp
// Пример использования Fluent API в коде компилятора
auto diag =
    DiagnosticBuilder (DiagCode::ERedefinition, "variable 'x' is already defined", DiagSeverity::Error)
        .AddAnnotation (firstDeclSpan, "previous definition was here", /*isPrimary=*/false)
        .AddAnnotation (secondDeclSpan, "redefined here", /*isPrimary=*/true)
        .AddNote ("identifiers within the same scope must be unique");
```

Вся информация о проблеме аккуратно упакована в единственный объект `diag`. Осталось передать его подсистеме
отрисовки, которая превратит эти структуры в стильный консольный вывод!
