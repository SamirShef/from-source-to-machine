# Текст в кавычках

## Введение

Помимо чисел Pebble поддерживает **строковые литералы**. Согласно спецификации Pebble строковые литералы ---
текст, окруженный двойными кавычками (`"Hello world!"`). Нельзя писать текст с переводом на новую строку
внутри строкового литерала:

```pebble
"Hello
world!"
```

выдаст ошибку. Плюс ко всему строковые литералы поддерживают escape-последовательности:
`\n`, `\r`, `\t`, `\\`, `\'`, `\"`, `\0`, `\b`, `\a`, `\f`, `\v`, `\xhh`
(где `h` --- шестнадцатиричная цифра), `\uhhhh` (где `h` --- шестнадцатиричная цифра), `\oOOO`
(где `O` --- восьмеричная цифра). `"Hello\nworld!"`.

Для лексера escape-последовательности не имеют никакого значения, как и суффиксы чисел, но в этом случае
лексер должен проверять последовательности на корректность. Если последовательность некорректна, то
лексер выдает ошибку. Плюс ко всему если лексер не будет ничего знать про последовательности, то строка
`"\""` токенизируется неправильно: строка закончится сразу после символа `\`, а при следующем вызове
`NextToken` откроется новая строка и не закроется. Лексер должен правильно пропускать эти символы.
Токен строкового литерала экранируется только под конец, когда нужно будет генерировать код.

## Новые токены

Добавим токен строкового литерала (`TokenKind::StrLit`). В `Val` этого токена будут храниться и
**кавычки в том числе**.

```cpp
// include/pebble/lexer/token_kind.h

// ...

enum class TokenKind : std::uint8_t {
    // ...
    NumLit,  // Числовой литерал
    StrLit,  // Строковый литерал
    // ...
};
```

## Токенизация

```cpp
// include/pebble/lexer/lexer.h

// ...

class Lexer {
    // ...
    void
    skipNumSuffix ();

    Token
    tokenizeStrLit ();
    // ...
};
```

```cpp
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::NextToken () {
    // ...
    if (peek () == '\"') {
        return tokenizeStrLit ();
    }
}

// ...

Token
Lexer::tokenizeStrLit () {
    auto start = _pos;
    advance (); // Пропускаем "
    while (!isAtEnd () && peek () != '\"') {
        advance ();
    }
    auto tokSpan = span (start, _pos);
    if (isAtEnd ()) {
        _diag
            .Report (
                diagnostic::DiagCode::EUnclosedStrLit,
                "unclosed string literal",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (tokSpan);
    } else {
        advance (); // Пропускаем "
    }
    return tok2 (StrLit, std::string_view (&_source[start], _pos - start), tokSpan);
}
```

Для начала нужно пропустить открывающую `"`. После этого необходимо пропускать все символы до ещё одной `"`
--- она должна закрывать литерал. Однако не забываем, что лексер может уйти до конца файла. Если литерал
дошел до конца файла и так и не встретилась закрывающая `"`, то выводится ошибка `unclosed string literal`.
Например, код:

```pebble
"Unclosed literal
```

Должен выдать ошибку, потому что `tokenizeStrLit` дойдет до конца файла (`if (isAtEnd ())` вернёт `true`).
Однако если конец файла не встретился (литерал закрывается корректно), то пропускаем закрывающую `"`.

Эта реализация корректно отработает на простых строках, по типу:

```pebble
"Hello, dear reader!"
```

### Запрет переноса строки

Как только мы попробуем что-то сломать, например,

```pebble
"Hello,
dear reader!"
```

То всё пойдёт не так. Во первых перевод на новую строку внутри литерала запрещён. Зачем это запрещать?
На это есть несколько причин:

1. Если бы перенос был разрешен, случайное нажатие клавиши Enter или пропущенная закрывающая кавычка не
вызывали бы моментальную ошибку компиляции. Компилятор считал бы весь последующий код частью строки, пока не
встретит следующую кавычку. Это приводило бы к огромным, трудночитаемым багам, ломающим логику программы на
сотни строк вперед.
2. Разные операционные системы используют разные невидимые символы для переноса строки. Для Windows ---
`\r\n`, для Linux/macOS --- `\n`. Из-за этого одна и та же строка на разных машинах может иметь разный размер
и содержимое, что нарушило бы кроссплатформенность.

Нужно сделать так, чтобы токенизация строки останавливалась, если встретится символ переноса строки.

```diff
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::tokenizeStrLit () {
    // ...
-   while (!isAtEnd () && peek () != '\"') {
+   while (!isAtEnd () && peek () != '\"' && peek () != '\n' && peek () != '\r') {
        advance ();
    }
    // ...
}
```

Если внутри литерала встречается перевод на новую строку, то цикл останавливается и выбрасывается ошибка
`unclosed string literal`.

Теперь код

```pebble
"Hello,
dear reader!"
```

Выдает две красивые ошибки:

```text
error[E0002]: unclosed string literal
 --> test.pebble:1:1
  |
1 | "Hello,
  | ^^^^^^^ 
  |

error[E0002]: unclosed string literal
 --> test.pebble:2:13
  |
2 | dear reader!"
  |             ^ 
  |
```

Появилась вторая ошибка, потому что для лексера `"` на 2-ой строке встречается впервые, а значит она
анализируется как начало строковго литерала. А так как после неё ничего нет (конец файла), то и закрыться
литерал также не может, что приводит к ошибке `unclsoed string literal`.

### Escape-последовательности

Мы так и не решили вопрос с escape-последовательностями. Причем важно, что внутри неё также может встретиться
перевод на новую строку, например,

```pebble
"Hello, dear reader!\
"
```

Для пропуска escape-последовательности создадим новый метод `skipEscapeSequence`.

```cpp
// include/pebble/lexer/lexer.h

// ...

class Lexer {
    // ...
    Token
    tokenizeStrLit ();

    void
    skipEscapeSequence ();
    // ...
};
```

```diff
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::tokenizeStrLit () {
    // ...
    while (!isAtEnd () && peek () != '\"' && peek () != '\n' && peek () != '\r') {
-       advance ();
+       skipEscapeSequence ();
    }
    // ...
}
```

```cpp
// src/lib/lexer/lexer.cpp

// ...

void
Lexer::skipEscapeSequence () {
    auto start = _pos;
    auto c     = advance ();
    if (c == '\\') {
        // Проверяем, встретится ли внутри последовательности перевод на новую строку
        if (isAtEnd () || peek () == '\n' || peek () == '\r') {
            _diag
                .Report (
                    diagnostic::DiagCode::EInvalidEscapeSequence,
                    "unfinished escape sequence",
                    diagnostic::DiagSeverity::Error)
                .AddAnnotation (
                    span (start, _pos),
                    "expected escape character before newline");
            return;
        }

        switch (advance ()) {
        case 'n':
        case 'r':
        case 't':
        case '\\':
        case '\'':
        case '\"':
        case '0':
        case 'b':
        case 'a':
        case 'f':
        case 'v':
            break;
        case 'x':
        case 'u':
        case 'o':
            // TODO: Реализуем чуть позже
            break;
        default:
            // Неизвестная последовательность
            _diag
                .Report (
                    diagnostic::DiagCode::EInvalidEscapeSequence,
                    std::string ("invalid escape sequence '\\") + peek (-1) + "'",
                    diagnostic::DiagSeverity::Error)
                .AddAnnotation (span (start, _pos), "unknown escape sequence");
            break;
        }
    }
}
```

`skipEscapeSequence` пропускает escape-последовательность или обычный символ. Если пропускаемый символ ---
`\`, то перед нами 100% escape-последовательность. Символ сразу после `\` определяет саму последовательность.
Мы пока что реализовали только пропуск односимвольных последовательностей. Если последовательность не
определена, то создаем ошибку.

Чтобы сделать пропуск мультисимвольных последовательностей (`\xhh`, `\uhhhh`, `\oOOO`), мы сделаем простые
методы, которые пропускают определенное количество цифр нужной системы счисления.

```cpp
// include/pebble/lexer/lexer.

// ...

class Lexer {
    // ...
    constexpr bool
    isAtEnd () {
        return _pos >= _source.size ();
    }

    static constexpr bool
    isHexDigit (char c) noexcept {
        return (c >= '0' && c <= '9')
            || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
    }

    static constexpr bool
    isOctalDigit (char c) noexcept {
        return c >= '0' && c <= '7';
    }

    void
    checkAndConsumeHexDigits (int digitCount);

    void
    checkAndConsumeUnicodeDigits (int digitCount);

    void
    checkAndConsumeOctalDigits (int digitCount);
};
```

```cpp
// src/lib/lexer/lexer.cpp

// ...

void
Lexer::checkAndConsumeHexDigits (int digitCount) {
    auto start = _pos - 1;
    int  count = 0;
    while (!isAtEnd () && isHexDigit (peek ())) {
        advance ();
        ++count;
    }

    if (count < digitCount) {
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidEscapeSequence,
                "invalid hex escape",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos), "expected 2 hex digits");
    }
}

void
Lexer::checkAndConsumeUnicodeDigits (int digitCount) {
    auto start = _pos - 1;
    int  count = 0;
    while (!isAtEnd () && isHexDigit (peek ())) {
        advance ();
        ++count;
    }

    if (count < digitCount) {
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidEscapeSequence,
                "invalid unicode escape",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos), "expected 4 hex digits");
    }
}

void
Lexer::checkAndConsumeOctalDigits (int digitCount) {
    auto start = _pos - 1;
    int  count = 0;
    while (!isAtEnd () && isOctalDigit (peek ())) {
        advance ();
        ++count;
    }

    if (count < digitCount) {
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidEscapeSequence,
                "invalid octal escape",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos), "expected 3 octal digits");
    }
}
```

Мы пропускаем цифры, которые подходят по контексту (для `\x` и `\u` пропускаем шестнадцатиричные цифры,
для `\o` --- восьмеричные). Причем количество пропускаемых цифр задается аргументом `int digitCount`.

Теперь реализация `skipEscapeSequence` меняется до:

```diff
// src/lib/lexer/lexer.cpp

// ...

void
Lexer::skipEscapeSequence () {
    // ...
    if (c == '\\') {
        // ...
        switch (advance ()) {
        // ...
        case 'x':
+           checkAndConsumeHexDigits (2);
+           break;
        case 'u':
+           checkAndConsumeUnicodeDigits (4);
+           break;
        case 'o':
-           // TODO: Реализуем чуть позже
+           checkAndConsumeOctalDigits (3);
            break;
        // ...
        }
    }
}
```
