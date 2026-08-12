# Тотальный игнор

Как мы уже и выяснили, лексер должен пропускать ненужные символы: пробелы и комментарии. В методе `NextToken`
нужно пропускать пробельные символолы и комментарии.

## Невидимки

Начнем с пробелов.

```cpp
// include/pebble/lexer/lexer.h

$#pragma once
$#include "pebble/diagnostic/engine.h"
$#include "pebble/lexer/token.h"
$
$namespace pebble {
$
class Lexer {
    // ...
private:
    void
    skipSpaces ();
    // ...
};
$
$}
```

Метод `skipSpaces` будет пропускать любые пробельные символы в кодировке ASCII.

```cpp
// src/lib/lexer/lexer.cpp

void
Lexer::skipSpaces () {
    while (std::isspace (static_cast<unsigned char> (peek ())) != 0) {
        advance ();
    }
}
```

`skipSpaces` проверяет текущий символ через `std::isspace` (является ли символ пробельным в кодировке ASCII).
Пока текущий символ пробельный, `skipSpaces` пропускает их. Теперь нужно научить `NextToken` вызывать
`skipSpaces`:

```cpp
// src/lib/lexer/lexer.cpp

Token
Lexer::NextToken () {
    // ...
    if (std::isspace (static_cast<unsigned char> (peek ())) != 0) {
        skipSpaces ();
        return NextToken ();
    }
}
```

После пропуска пробелов мы снова вызываем `NextToken`, чтобы начать разбор токенов заново. Например, если
лексер получит строку `"   "`, то:

1. `if (peek () == '\0')` вернет `false`.
2. `if (std::isspace (peek ()) != 0)` вернет `true`.
3. Вызовется `skipSpaces` и пропустятся все пробелы.
4. Снова вызовется `NextToken`.
5. `if (peek () == '\0')` вернет `true` -> вернется токен `Eof`.

Если бы мы убрали `return NextToken ();`, то после шага 3 код просто дошел до конца функции и всё закончилось
бы `illegal instruction`.

## Заметки программиста

Комментарии пропускать будет чуть сложнее, потому что комментарии могут быть однострочными и многострочными.
В Pebble комментарии будут как в `C` (`//` --- однострочные, `/* */` --- многострочные).

```cpp
// include/pebble/lexer/lexer.h

$#pragma once
$#include "pebble/diagnostic/engine.h"
$#include "pebble/lexer/token.h"
$
$namespace pebble {
$
class Lexer {
    // ...
private:
    void
    skipComment ();

    void
    skipMultilineComment ();

    void
    skipSingleComment ();

    void
    skipSpaces ();
    // ...
};
$
$}
```

`skipComment` является мостом между `skipSingleComment` и `skipMultilineComment`

```cpp
// src/lib/lexer/lexer.cpp

void
Lexer::skipComment () {
    advance (); // Пропускаем первый '/'
    bool isMultiline = advance () == '*';
    if (isMultiline) {
        skipMultilineComment ();
    } else {
        skipSingleComment ();
    }
}

void
Lexer::skipMultilineComment () {
    while (peek (-1) != '/' || peek (-2) != '*') {
        advance ();
    }
}

void
Lexer::skipSingleComment () {
    while (advance () != '\n') {}
}
```

Комментарии (однострочные или многострочные) всегда начинаются с `/`, поэтому `skipComment` всегда пропускает
его. Чтобы `skipComment` понял тип комментария, он должен посмотреть на следующий за пропущенным `/` символ.
Если символ после `/` это `*`, то комментарий многострочный. В противном случае
(после `/` идет ещё один `/`) однострочный.

`skipSingleComment` пропускает символы до тех пор, пока не
закончится текущая строка. Цикл внутри `skipSingleComment` пропускает символы до конца текущей строки и
одновременно с этим пропустит сам символ перевода на новую строку --- если `advance ()` вернет `\n`, то
это значит, что он пропустил его, и цикл остановится на этом моменте.

В свою очередь `skipMultilineComment` проверяет паттерн `*/`. Если паттерн не наблюдается, то пропускаются
символы.

Теперь научим `NextToken` видеть начало комментариев и пропускать их:

```cpp
// src/lib/lexer/lexer.cpp

Token
Lexer::NextToken () {
    // ...
    if (peek () == '/' && (peek (1) == '/' || peek (1) == '*')) {
        skipComment ();
        return NextToken ();
    }
    // ...
}
```

Как только `NextToken` видит паттерны `//` или `/*`, то сразу понимает, что это комментарий, вызывает
`skipComment` и рекурсивно вызывает `NextToken`, как и с пробелами.

### Вложенность комментариев

Комментарии в Pebble не могут быть вложенными, например:

```pebble
/*
    /*

    */
*/
```

Лексер подумает, что паттерн `*/` на 4 строке закрывает многострочный комментарий, открытый в 1 строке. Из-за
этого в 5 строке он сгенерирует два токена: `*` и `/`.

Если вы хотите поддержку вложенных комментариев, то внутри `skipSingleComment` и `skipMultilineComment`
нужно проверять то же самое, что и в `NextToken` и рекурсивно вызывать `skipComment`.

## Фатальная ошибка

Кажется, что такой простой этап уже позади, но нет. Лексер запросто уйдёт в вечный цикл, как только ему
попадется код:

```pebble
// comment
```

или

```pebble
/*
    unclosed comment
```

Проблема не очевидна --- когда цикл упрётся в конец файла, `peek ()` и `advance ()` начнут возвращать `\0`.
Из-за того, что мы не проверяем выходы за пределы буфера (файла), цикл не остановится.

Исправить это можно добавлением метода `isAtEnd ()` и договоренностью --- **каждый `while` в лексере должен
проверять, что он не выходит за пределы буфера (`!isAtEnd ()`)**.

```cpp
// include/pebble/lexer/lexer.h

$#pragma once
$#include "pebble/diagnostic/engine.h"
$#include "pebble/lexer/token.h"
$
$namespace pebble {
$
class Lexer {
    // ...
    char
    peek (int relPos = 0) const;

    constexpr bool
    isAtEnd () {
        return _pos >= _source.size ();
    }
};
$
$}
```

```diff
// src/lib/lexer/lexer.cpp

Token
Lexer::NextToken () {
-   if (peek () == '\0') {
+   if (isAtEnd ()) {
        return tok2 (Eof, "", span (_pos, _pos));
    }
    // ...
}

// ...

void
Lexer::skipMultilineComment () {
-   while (peek (-1) != '/' || peek (-2) != '*') {
+   while (!isAtEnd () && (peek (-1) != '/' || peek (-2) != '*')) {
        advance ();
    }
}

void
Lexer::skipSingleComment () {
-   while (advance () != '\n') {}
+   while (!isAtEnd() && advance () != '\n') {}
}

void
Lexer::skipSpaces () {
-   while (std::isspace (static_cast<unsigned char> (peek ())) != 0) {
+   while (!isAtEnd () && std::isspace (static_cast<unsigned char> (peek ())) != 0) {
        advance ();
    }
}
```
