# Ошибаемся с удовольствием

## Введение

Мы прошли большой путь в этой главе:

1. Создали легкие структуры координат (`Pos` и `Span`).
2. Построили `SourceMgr`, гарантирующий владение памятью файлов и быстрый поиск координат за `O(log N)`.
3. Разработали удобный `DiagnosticBuilder` с Fluent API.
4. Написали `DiagnosticEngine` — «художника», выравнивающего вырезки кода и создающего красивый ANSI-вывод.

Пришла пора проверить нашу подсистему в реальном бою! В этом разделе мы разберём **5 практических примеров** генерации и рендеринга диагностик — от простых синтаксических ошибок до комплексных семантических сбоев с несколькими аннотациями и примечаниями.

## Галерея диагностических сообщений

### Пример 1. Незакрытая строковая константа (`EUnclosedStrLit`)

Простейший случай: лексер встречает открывающую кавычку `"`, но доходит до конца строки или файла, так и не встретив закрывающую.

```cpp
$#include "pebble/basic/source_mgr.h"
$#include "pebble/diagnostic/engine.h"
$
$using namespace pebble;
$using namespace pebble::basic;
$using namespace pebble::diagnostic;
$
void
DemoUnclosedString (SourceMgr &srcMgr, DiagnosticEngine &engine) {
    uint32_t fileId = srcMgr.AddFile (
        "src/test1.pebble",
        "fn main() {\n"
        "    var msg = \"hello world;\n"
        "}\n");

    // Указываем диапазон открытой строки: от кавычки " до конца строки
    Pos start{ 26, fileId }; // строка 2, колонка 15
    Pos end{ 40, fileId };   // строка 2, колонка 28

    engine
        .Report (
            DiagCode::EUnclosedStrLit,
            "unclosed string literal",
            DiagSeverity::Error)
        .AddAnnotation (Span{ start, end }, "string literal starts here", true);
}
$
$int
$main () {
$    EnableVirtualTerminalProcessing ();
$    basic::SourceMgr             mgr;
$    diagnostic::DiagnosticEngine diag (mgr);
$    DemoUnclosedString (mgr, diag);
$    diag.Render ();
$    return 0;
$}
```

Вывод в консоли:

```text
error[E0002]: unclosed string literal
 --> src/lexer.pebble:2:15
  |
2 |     var msg = "hello world;
  |               ^^^^^^^^^^^^^ string literal starts here
  |
```

### Пример 2. Неиспользуемая переменная (`WUnusedVar`)

Предупреждение компилятора. Обратите внимание: функция `DiagCodeToIntegerCode` автоматически вычитает смещение `WARN_CODE_START`, поэтому код предупреждения выводится как `W0000`, а внизу добавляется подсказка `Note`.

```cpp
void
DemoUnusedVariable (SourceMgr &srcMgr, DiagnosticEngine &engine) {
    uint32_t fileId = srcMgr.AddFile (
        "src/math.pebble",
        "fn calculate(): int32 {\n"
        "    var unused_val = 42;\n"
        "    return 0;\n"
        "}\n");

    Pos start{ 32, fileId }; // строка 2, колонка 9
    Pos end{ 42, fileId };   // строка 2, колонка 19

    engine
        .Report (
            DiagCode::WUnusedVar,
            "unused variable 'unused_val'",
            DiagSeverity::Warning)
        .AddAnnotation (Span{ start, end }, "variable declared here", true)
        .AddNote (
            "if this is intentional, prefix the variable name with an underscore: "
            "'_unused_val'");
}
```

Вывод в консоли:

```text
warning[W0000]: unused variable 'unused_val'
 --> src/math.pebble:2:9
  |
2 |     var unused_val = 42;
  |         ^^^^^^^^^^ variable declared here
  |
  = note: if this is intentional, prefix the variable name with an underscore: '_unused_val'
```

### Пример 3. Неверный суффикс числового литерала (`EInvalidNumSuffix`)

Ошибка анализатора чисел: программист случайно приписал невалидный суффикс `abc` к целому числу.

```cpp
void
DemoInvalidSuffix (SourceMgr &srcMgr, DiagnosticEngine &engine) {
    uint32_t fileId = srcMgr.AddFile (
        "src/main.pebble",
        "fn main(): void {\n"
        "    var num = 123abc;\n"
        "}\n");

    Pos suffixStart{ 35, fileId }; // строка 2, колонка 18
    Pos suffixEnd{ 38, fileId };   // строка 2, колонка 21

    engine
        .Report (
            DiagCode::EInvalidNumSuffix,
            "invalid suffix 'abc' on integer literal",
            DiagSeverity::Error)
        .AddAnnotation (Span{ suffixStart, suffixEnd }, "invalid suffix", true)
        .AddNote ("integer literals support suffixes like 'i32', 'u64', or 'f32'");
}
```

Вывод в консоли:

```text
error[E0006]: invalid suffix 'abc' on integer literal
 --> src/main.pebble:2:18
  |
2 |     var num = 123abc;
  |                  ^^^ invalid suffix
  |
  = note: integer literals support suffixes like 'i32', 'u64', or 'f32'
```

### Пример 4. Пропущенное выражение в синтаксическом дереве (`EExpectedExpr`)

Очередная ошибка парсера: бинарный оператор `+` ожидает правый операнд, но вместо него встречается точка с запятой `;`.

```cpp
void
DemoExpectedExpr (SourceMgr &srcMgr, DiagnosticEngine &engine) {
    uint32_t fileId = srcMgr.AddFile (
        "src/parser.pebble",
        "fn main(): void {\n"
        "    var val = 10 + ;\n"
        "}\n");

    Pos semiLoc{ 37, fileId }; // строка 2, колонка 20 (точка с запятой)

    engine
        .Report (
            DiagCode::EExpectedExpr,
            "expected expression after '+' operator",
            DiagSeverity::Error)
        .AddAnnotation (
            Span{
                semiLoc,
                Pos{ 38, fileId }
    },
            "expected expression here",
            true);
}
```

Вывод в консоли:

```text
error[E0001]: expected expression after '+' operator
 --> src/parser.pebble:2:20
  |
2 |     var val = 10 + ;
  |                    ^ expected expression here
  |
```

### Пример 5. Комплексный конфликт типов (`EIntSuffixForFloat`)

Изумительный пример работы двойной аннотации! Семантический анализатор обнаруживает, что целочисленный суффикс `i32` был применён к числу с плавающей точкой `3.14`.

Здесь выравнивание, первичная (`^`) и вторичная (`-`) аннотации работают вместе, чтобы дать точечное пояснение:

```cpp
void
DemoIntSuffixForFloat (SourceMgr &srcMgr, DiagnosticEngine &engine) {
    uint32_t fileId = srcMgr.AddFile (
        "src/types.pebble",
        "fn main(): void {\n"
        "    var pi = 3.14i32;\n"
        "}\n");

    Pos floatStart{ 31, fileId }; // "3.14"
    Pos floatEnd{ 35, fileId };

    Pos suffixStart{ 35, fileId }; // "i32"
    Pos suffixEnd{ 38, fileId };

    engine
        .Report (
            DiagCode::EIntSuffixForFloat,
            "integer suffix 'i32' cannot be applied to floating-point literal '3.14'",
            DiagSeverity::Error)
        .AddAnnotation (
            Span{ floatStart, floatEnd },
            "floating-point literal here",
            false) // Вторичная (-)
        .AddAnnotation (
            Span{ suffixStart, suffixEnd },
            "invalid suffix",
            true) // Первичная (^)
        .AddNote ("use floating-point suffixes like 'f32' or 'f64' instead");
}
```

Вывод в консоли:

```text
error[E0005]: integer suffix 'i32' cannot be applied to floating-point literal '3.14'
 --> src/types.pebble:2:14
  |
2 |     var pi = 3.14i32;
  |              ---- floating-point literal here
  |
2 |     var pi = 3.14i32;
  |                  ^^^ invalid suffix
  |
  = note: use floating-point suffixes like 'f32' or 'f64' instead
```

## Итоги второй главы

Мы закончили построение фундаментальной инфраструктуры компилятора **Pebble**.

Давайте подведём итог того, что мы создали в этой главе:

* **Нулевые копирования (Zero-Copy):** `SourceMgr` хранит файлы в памяти на протяжении всего времени работы программы, позволяя передавать `std::string_view` сквозь все фазы компиляции.
* **Быстрый поиск за `O(log N)`:** Благодаря бинарному поиску по массиву `LineStarts`, пересчёт абсолютных байтовых смещений `Pos` в номера строк и колонок `Loc` выполняется мгновенно.
* **Гибкий Fluent API:** Класс `DiagnosticBuilder` позволил формировать сложнейшие ошибки с множеством аннотаций в одну лаконичную цепочку вызовов.
* **Профессиональный рендер:** `DiagnosticEngine` гарантирует красивую выровненную верстку с цветовой подсвечивающей индикацией, превосходящую стандартные текстовые ошибки большинства языков.

Система диагностики — это лицо нашего компилятора. Теперь любое нарушение правил языка Pebble будет встречено не молчаливым падением, а понятным, дружелюбным и визуально красивым сообщением.

## Что дальше?

С фундаментом окончено. У нас есть позиционирование, файлы и мощная система ошибок. Пришла пора вдохнуть жизнь в текст исходного кода!

В следующей главе — **«Глава 3. Разрезая текст: Лексический анализ»** — мы перейдём к написанию первого настоящего этапа компилятора: **Лексера (Сканера)**. Мы научимся нарезать сырой поток байтов на атомарные кирпичики языка — **токены**, обработаем ключевые слова, строки, комментарии и числовые литералы.

До встречи в мире лексем!
