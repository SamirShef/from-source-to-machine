# Пунктуация

## Введение

Лексер почти готов, осталось только добавить токенизацию специальных символов. Специальные символы (или же
**пунктуация**) --- набор специальных символов и знаков препинания, которые определяют структуру кода,
разделяют команды и помогают компилятору правильно понять написанные инструкции. Например, распространённые
символы пунктуации в C-подобных языках: `;`, `{`, `}`. Символ пунктуации, в зависимости от контекста,
может легко стать оператором (например, `+`). Символы пунктуации не обязательно должны быть односимвольными
(как `=`, `+`, `;` и т. д.). Они могут быть в длину и два (`==`, `+=`, `>=`), и три символа (`===` в `JS`).

В некоторых компиляторах, например, `clang` и `rustc`, пунктуация анализируется прямо в главном методе
токенизации (аналог нашего `NextToken`). У `clang` это метод `Lexer::LexTokenInternal`, а для `rustc` ---
`Cursor::advance_token`. Мы же поступим более деликатно --- сделаем отдельный метод `Lexer::tokenizePunct`,
который будет токенизировать всю пунктуацию внутри себя, не засоряя `NextToken`, чтобы вам было легче читать.
Зачем компиляторы не создают отдельных методов (вроде `tokenizeId`, `tokenizePunct`)? Чтобы обеспечить
бóльшую производительность, уменьшив тем самым накладные расходы на вызовы функций.

Наш метод `tokenizePunct` помимо токенизации пунктуации будет ещё и возвращать `Token (TokenKind::Unknown)`,
если символ не входит в алфавит языка. Таким образом мы полностью закроем лексический анализ.

## Новые токены

Каждый символ пунктуации --- отдельный тип токена. Определим пока что самые базовые, чтобы вы поняли как
добавлять новые.

```cpp
// include/pebble/lexer/token_kind.h

// ...

enum class TokenKind : std::uint8_t {
    // ...
    StrLit, // Строковый литерал

    Semi,      // `;`
    Comma,     // `,`
    Eq,        // `=`
    Amp,       // `&`
    Pipe,      // `|`
    AmpAmp,    // `&&`
    PipePipe,  // `||`
    Plus,      // `+`
    Minus,     // `-`
    PlusEq,    // `+=`
    MinusEq,   // `-=`
    AmpEq,     // `&=`
    PipeEq,    // `|=`
    EqEq,      // `==`
    // ...
};
```

## Реализация

Вся суть проверки операторов лексером --- проверять текущий и следующие символы. Тут снова работает Maximal
Munch. Лексер должен вернуть самую длинную вариацию оператора. Например, для текста `+=` лексер не должен
вернуть `Token (TokenKind::Plus)`, а затем `Token (TokenKind::Eq)`. Он должен вернуть
`Token (TokenKind::PlusEq)` (съесть максимум символов, которые складываются в языковую конструкцию).

```cpp
// include/pebble/lexer/lexer.h

// ...

class Lexer {
    // ...

    Token
    tokenizeCharLit ();

    Token
    tokenizePunct ();

    // ...
};

// ...
```

Наконец-то мы можем завершить `NextToken`:

```cpp
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::NextToken () {
    // ...
    if (peek () == '\'') {
        return tokenizeCharLit ();
    }
    return tokenizePunct ();
}

// ...
```

Пунктуация --- последнее, что будет токенизировать лексер.

Давайте абстрактно подумаем, как токенизировать пунктуацию. Самое главное --- проверить текущий символ.
На его основе будет понятно что делать дальше. Для этого можно сделать большой `switch`:

```cpp
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::tokenizePunct () {
    auto start = _pos;
    auto kind  = TokenKind::Unknown;

    char ch = advance ();
    switch (ch) {
        // TODO: скоро добавим сюда case'ы
    default:
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidChar,
                std::string ("unknown character '") + ch + "'",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos));
        break;
    }

    return tok (
        kind,
        std::string_view (&_source[start], _pos - start),
        span (start, _pos));
}

// ...
```

`DiagCode::EInvalidChar` --- это новый код диагностики (неизвестный символ):

```diff
// include/diagnostic/codes.h

// ...

enum class DiagCode : std::uint8_t {
    // ...
    EInvalidEscapeSequence,
+   EInvalidChar,
    // ...
};

// ...
```

В `switch`-е нужно перечислить все символы, с которых может начинаться символ пунктуации.
Например:

```cpp
case ';':
    kind = TokenKind::Semi;
    break;
```

Но если символ в `case` --- начало нескольких символов пунктуации, например, `+`, то в блоке `case` нужно
создавать проверки на впереди идущий символ (Maximal Munch):

```cpp
case '+':
    if (peek () == '=') {
        advance (); // Пропускаем `=`
        kind = TokenKind::PlusEq;
    } else {
        kind = TokenKind::Plus;
    }
    break;
```

Почему `peek ()`, а не `peek (1)`? Потому что `switch` проходится по переменной `ch`, которая в свою очередь
определяется как `char ch = advance ();`. То есть первый символ всегда пропускается, из-за чего `peek ()`
возвращает **символ сразу за ch**.

Но есть символы, которые могут быть началом сразу трёх символов пунктуации, например, `&` (`&`, `&&`, `&=`).
Тут тоже генерируем проверки, только уже два условия:

```cpp
case '&':
    if (peek () == '&') {
        advance (); // Пропускаем `&`
        kind = TokenKind::AmpAmp;
    } else if (peek () == '=') {
        advance (); // Пропускаем `=`
        kind = TokenKind::AmpEq;
    } else {
        kind = TokenKind::Amp;
    }
    break;
```

Но согласитесь, все эти одинаковые `case` будут смотреться очень вырвеглазно. Почему бы тогда не обернуть их
в макросы? Так и сделаем! Пусть будет макрос, который генерирует `case` для односимвольного знака пунктуации
(`single`), макрос для знака пунктуации на два варианта (`pair`) и на три варианта (`triple`):

```cpp
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::tokenizePunct () {
    // ...

#define single(ch, tok_kind)                                                             \
    case ch:                                                                             \
        kind = TokenKind::tok_kind;                                                      \
        break;

#define pair(ch, next_ch, tok_match, tok_default)                                        \
    case ch:                                                                             \
        if (peek () == (next_ch)) {                                                      \
            advance ();                                                                  \
            kind = TokenKind::tok_match;                                                 \
        } else {                                                                         \
            kind = TokenKind::tok_default;                                               \
        }                                                                                \
        break;

#define triple(ch, ch1, tok1, ch2, tok2, tok_default)                                    \
    case ch:                                                                             \
        if (peek () == (ch1)) {                                                          \
            advance ();                                                                  \
            kind = TokenKind::tok1;                                                      \
        } else if (peek () == (ch2)) {                                                   \
            advance ();                                                                  \
            kind = TokenKind::tok2;                                                      \
        } else {                                                                         \
            kind = TokenKind::tok_default;                                               \
        }                                                                                \
        break;

    char ch = advance ();
    // ...

#undef triple
#undef pair
#undef single
    // ...
}

// ...
```

Эти макросы полностью повторяют наши паттерны по `case`-ам. Теперь, используя эти макросы, можно легко
добавлять новые знаки пунктуации.

```diff
// src/lib/lexer/lexer.cpp

// ...

Token
Lexer::tokenizePunct () {
    // ...

    char ch = advance ();
    switch (ch) {
-       // TODO: скоро добавим сюда case'ы
+       single (';', Semi);
+       single (',', Comma);
+       pair ('=', '=', EqEq, Eq);
+       pair ('+', '=', PlusEq, Plus);
+       pair ('-', '=', MinusEq, Minus);
+       triple ('&', '=', AmpEq, '&', AmpAmp, Amp);
+       triple ('|', '=', PipeEq, '|', PipePipe, Pipe);
    default:
        // ...
    }

    // ...
}

// ...
```

По мере обновления Pebble будем добавлять новые знаки пунктуации.
