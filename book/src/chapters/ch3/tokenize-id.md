# Имена и ключевые слова

## Введение

Теперь нужно научить лексер токенизировать ключевые слова и идентификаторы. Идентификаторы с точки зрения
лексики --- это набор символов букв, цифр и `_`. Но есть важное правило: идентификатор не может начинаться
с цифры, потому что с цифры может начинаться только числовой литерал. Именно отсюда и взялось это правило.
Как правило идентификаторы нужны в языке, чтобы именовать символы в языке программирования (переменные,
функции, структуры и т. д.). Ключевые слова --- это зарезервированные идентификаторы, которые имеют
определенный синтаксический смысл.

Например, в коде:

```pebble
var x = 10;
```

`var` --- ключевое слово (начало определения переменной), а `x` --- идентификатор (имя переменной).

## Новые токены

Ключевые слова представляются отдельными `TokenKind`, поэтому для начала дополним список
токенов. Спецификация Pebble устанавливает следующий список токенов: `bool`, `char`, `int8`, `int16`,
`int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`, `float32`, `float64`, `string`, `true`, `false`,
`var`, `const`, `fn`, `return`, `if`, `else`, `for`, `break`, `continue`, `struct`, `nil`, `import`,
`extern`. Важно подметить, что `true`/`false` являются **ключевыми словами и литералами одновременно**.
В коде Pebble `true`/`false` хранятся как `BoolLit` (булевый литерал), но при этом обрабатываются лексером
как ключевое слово.

```cpp
// include/pebble/lexer/token_kind.h

// ...

enum class TokenKind : std::uint8_t {
    Id, // Идентификатор

    Bool,    // Ключевое слово `bool`
    Char,    // Ключевое слово `char`
    Int8,    // Ключевое слово `int8`
    Int16,   // Ключевое слово `int16`
    Int32,   // Ключевое слово `int32`
    Int64,   // Ключевое слово `int64`
    Uint8,   // Ключевое слово `uint8`
    Uint16,  // Ключевое слово `uint16`
    Uint32,  // Ключевое слово `uint32`
    Uint64,  // Ключевое слово `uint64`
    Float32, // Ключевое слово `float32`
    Float64, // Ключевое слово `float64`
    String,  // Ключевое слово `string`

    Var,      // Ключевое слово `var`
    Const,    // Ключевое слово `const`
    Fn,       // Ключевое слово `fn`
    Ret,      // Ключевое слово `return`
    If,       // Ключевое слово `if`
    Else,     // Ключевое слово `else`
    For,      // Ключевое слово `for`
    Break,    // Ключевое слово `break`
    Continue, // Ключевое слово `continue`
    Struct,   // Ключевое слово `struct`
    Nil,      // Ключевое слово `nil`
    Import,   // Ключевое слово `import`
    Extern,   // Ключевое слово `extern`

    BoolLit,  // Булевый литерал (true/false)
    NumLit,   // Числовой литерал
    // ...
};
```

## Токенизация

Токен `TokenKind::Id` уже есть, так что можно перейти сразу к реализации в лексере.

```cpp
// include/pebble/lexer/lexer.h

// ...

class Lexer {
    // ...
private:
    Token
    tokenizeIdOrKeyword ();

    Token
    tokenizeNumLit ();
    // ...
};
```

```cpp
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::NextToken () {
    // ...
    if (std::isalpha (peek ()) != 0 || peek () == '_') {
        return tokenizeIdOrKeyword ();
    }
    // ...
}

// ...

Token
Lexer::tokenizeIdOrKeyword () {
    auto start = _pos;
    while (!isAtEnd () && (std::isalnum (peek ()) != 0 || peek () == '_')) {
        advance ();
    }
    std::string_view val (&_source[start], _pos - start);
    return tok (TokenKind::Id, val, span (start, _pos));
}
```

В `NextToken` мы должны проверить, является ли символ буквой или `_` --- это единственные валидные символы
начала идентификатора или ключевого слова. Для токенизации идентификатора или ключевого слова нужен метод
`tokenizeIdOrKeyword`, который просто съедает все символы букв, десятичных цифр и `_`. Но есть главная
проблема --- `tokenizeIdOrKeyword` создает **только** идентификаторы. Он ещё ничего не знает про
ключевые слова. Можно легко это исправить --- мы добавим хеш-таблицу (строка -> `TokenKind`) ключевых слов.

```cpp
// include/pebble/lexer/keywords.h

#pragma once
#include "pebble/lexer/token_kind.h"
#include <string_view>
#include <unordered_map>

namespace pebble {

#define keyword(val, kind) { (val), pebble::TokenKind::kind }

static inline const std::unordered_map<std::string_view, TokenKind> KEYWORDS{
    keyword ("false", BoolLit),
    keyword ("true", BoolLit),
    keyword ("var", Var),
    keyword ("const", Const),
    keyword ("bool", Bool),
    keyword ("char", Char),
    keyword ("int8", Int8),
    keyword ("uint8", Uint8),
    keyword ("int16", Int16),
    keyword ("uint16", Uint16),
    keyword ("int32", Int32),
    keyword ("uint32", Uint32),
    keyword ("int64", Int64),
    keyword ("uint64", Uint64),
    keyword ("float32", Float32),
    keyword ("float64", Float64),
    keyword ("string", String),
    keyword ("fn", Fn),
    keyword ("return", Ret),
    keyword ("if", If),
    keyword ("else", Else),
    keyword ("for", For),
    keyword ("break", Break),
    keyword ("continue", Continue),
    keyword ("struct", Struct),
    keyword ("nil", Nil),
    keyword ("import", Import),
    keyword ("extern", Extern)
};

#undef keyword

}
```

Макрос `keyword` --- сахар для добавления токена в хеш-таблицу.

Теперь, чтобы в `tokenizeIdOrKeyword` понять, вернуть токен идентификатора или ключевого слова, нужно
проверить, есть ли проанализированная строка в `KEYWORDS`. Если есть, то `tokenizeIdOrKeyword` возвращает
ключевое слово, в противном случае --- идентификатор.

```diff
// src/lib/lexer/lexer.cpp

// ...
+#include "pebble/lexer/keywords.h"

Token
Lexer::NextToken () {
    // ...
}

// ...

Token
Lexer::tokenizeIdOrKeyword () {
    auto start = _pos;
    while (!isAtEnd () && (std::isalnum (peek ()) != 0 || peek () == '_')) {
        advance ();
    }
    std::string_view val (&_source[start], _pos - start);
-   return tok (TokenKind::Id, val, span (start, _pos));
+   auto             kind = TokenKind::Id;
+   // Пытаемся найти val в KEYWORDS
+   if (auto it = KEYWORDS.find (val); it != KEYWORDS.end ()) {
+       // Нашли, значит возвращаем как ключевое слово из KEYWORDS
+       kind = it->second;
+   }
+    return tok (kind, val, span (start, _pos));
}
```
