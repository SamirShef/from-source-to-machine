# Одинокие символы

## Самая быстрая часть

Чем с точки зрения лексера отличается строковый литерал от символьного литерала? У строкового литерала
двойные кавычки (`"`), а у символьного --- одинарные (`'`).

```cpp
// include/pebble/lexer/token_kind.h

enum class TokenKind : std::uint8_t {
    // ...
    BoolLit, // Булевый литерал (true/false)
    CharLit, // Символьный литерал
    // ...
};
```

```cpp
// include/pebble/lexer/lexer.h

class Lexer {
    // ...
    Token
    tokenizeStrLit ();

    Token
    tokenizeCharLit ();
    // ...
};
```

```cpp
// src/lib/lexer/lexer.cpp

Token
Lexer::tokenizeCharLit () {
    auto start = _pos;
    advance (); // Пропускаем '
    while (!isAtEnd () && peek () != '\'' && peek () != '\n' && peek () != '\r') {
        skipEscapeSequence ();
    }
    auto tokSpan = span (start, _pos);
    if (isAtEnd () || peek () == '\n' || peek () == '\r') {
        _diag
            .Report (
                diagnostic::DiagCode::EUnclosedCharLit,
                "unclosed character literal",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (tokSpan);
    } else {
        advance (); // Пропускаем '
    }
    return tok2 (CharLit, std::string_view (&_source[start], _pos - start), tokSpan);
}
```

Всё работает точно так же, как и у строковых литералов. Поменялись лишь коды ошибок (`EUnclosedStrLit` ->
`EUnclosedCharLit`), кавычки (`"` -> `'`) и тип токена (`StrLit` -> `CharLit`).

Символьные литералы по спецификации Pebble могут иметь только один ASCII символ (как в `C`). Лексер же
может съесть несколько символов. Лексер не имеет права проверять длину литерала, потому что внутри него могут
быть escape-последовательности, которые по своей природе многосимвольные. Если бы лексер должен был проверять
длину, то его пришлось бы учить понимать смысл последовательностей. Но лексер, как мы помним, это просто
резак текста на токены. Не больше.

Но вы не расслабляйтесь --- больше таких коротких и простых глав не будет.
