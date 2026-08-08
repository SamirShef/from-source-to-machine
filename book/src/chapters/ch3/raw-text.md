# Безликий текст

## Введение

Лексер, как мы уже выяснили, очень простой и быстрый этап компиляции. Он просто разрезает текст на токены,
сохраняя их структуру в тексте.

Работа лексера сводится к простому сравнению символов. Обычно в компиляторах лексер за раз выдает по одному
токену (поток токенов), то есть вместо генерации списка токенов лексер возвращает по одному токену. Главная
причина возвращать токены по одному вместо списка --- память. Если мы пишем небольшую программу:

```pebble
fn main(): int32 {
    return 0;
}
```

То разницы в подходах (список vs. поток токенов) окажется несущественной. Но как только в одном файле будет
больше тысячи, а то и десятков тысяч строк, в подходе со списком начнется проблема --- он будет потреблять
слишком много памяти. Вторая причина --- банальная ненужность знать все токены за раз (то есть почти вся
выделенная память списка окажется просто ненужной). Если грамматика вашего языка контекстно независимая ---
грамматика, правила которой позволяют строить синтаксис без знаний контекста (в отличие от `C/C++`), --- то
компилятору достаточно знать всего 3 токена в каждый момент времени (токен, на который "смотрит" компилятор,
и по одному токену после него и перед ним). Из-за этого компилятору не нужно знать весь список токенов, пока
он строит синтаксис для каждой синтаксической конструкции. Если ему понадобиться пройти дальше (узнать новый
токен), он просто попросит об этом лексер, который на лету создаст новый токен. То есть лексер и
синтаксический анализатор работают в тандеме, но при этом не мешают друг другу.

## Токены

Токены должны быть легковесной структурой без лишней информации:

```cpp
// include/pebble/lexer/token.h

$#pragma once
$#include "pebble/diagnostic/span.h"
$#include "pebble/lexer/token_kind.h"
$#include <string_view>
$
$namespace pebble {
$
struct Token {
    TokenKind        Kind;
    std::string_view Val;
    diagnostic::Span Span;

    Token (TokenKind kind, std::string_view val, diagnostic::Span span)
        : Kind (kind), Val (val), Span (span) {}
};
$
$}
```

Значение токена будет храниться как `std::string_view`, вместо обычной `std::string`. Токен, как мы помним,
хранит свой вид из исходного кода. Файлы исходного кода живут в `SourceMgr`, а значит если брать слайс
из буферов, то он всегда будет валидным. Это позволит нам экономить память и ускорит компилятор.

`TokenKind` --- это тип токена.

```cpp
// include/pebble/lexer/token_kind.h

$#pragma once
$#include <cstdint>
$
$namespace pebble {
$
enum class TokenKind : std::uint8_t {
    Id, // Идентификатор

    Unknown,
    Eof
    // Остальные типы токенов будем добавлять по мере расширения грамматики
};
$
$}
```

Так как лексер выдает токены по одному вместо вектора, синтаксическому анализатору нужно знать когда
остановиться (перестать просить новые токены). Для этого в `TokenKind` можно добавить отдельный тип токенов
--- `Eof` (от End Of File). Синтаксический анализатор при виде токена с `TokenKind::Eof` завершает свою
работу.

Есть ещё один интересный `TokenKind` --- `Unknown`. Если лексика вашего языка не подразумевает наличие
каких-то символов (например, `$`, как в Pebble), то нужно научить лексер создавать валидный токен и
выкидывать ошибку. Для этого и существует `TokenKind::Unknown`.

И `TokenKind::Eof`, и `TokenKind::Unknown` также очень важны для синтаксического анализатора. Если написать
код с неизвестным для лексера символом, то синтаксический анализатор должен штатно скушать этот токен и не
сломаться. Синтаксический анализатор для построения синтаксиса сравнивает `TokenKind` текущего токена
(токена, на который он "смотрит" в данный момент времени). Если он понимает, что не ожидал увидеть переданный
токен, то просто выкидывает синтаксическую ошибку. `TokenKind::Eof` и `TokenKind::Unknown` идеально сюда
вписываются --- это полностью валидные токены с валидной позицией, из-за чего синтаксические ошибки также
будут корректно выглядеть и не нужно будет писать дополнительную логику на случай, если появится один из
этих двух токенов.

## Лексер

Начнем проектировать лексер. Лексер должен знать какой файл он анализирует и на каком смещении в нём он
находится (`_fileId` и `_pos`). Это нужно для позиционирования токенов. Чтобы лексер знал, какой символ
он анализирует, нужно передать ему содержимое файла (`_source`). А чтобы выбрасывать ошибки передадим ему
ещё и движок диагностики (по ссылке, чтобы состояние движка изменялось).

```cpp
// include/pebble/lexer/lexer.h

$#pragma once
$#include "pebble/diagnostic/engine.h"
$#include "pebble/lexer/token.h"
$
$namespace pebble {
$
class Lexer {
    diagnostic::DiagnosticEngine &_diag;
    std::uint32_t                 _fileId;
    std::uint32_t                 _pos{};
    const std::string            &_source;

public:
    Lexer (diagnostic::DiagnosticEngine &diag, std::uint32_t fileId)
        : _diag (diag),
          _fileId (fileId),
          _source (diag.SourceMgr ().GetFile (fileId).Content) {}

    Token
    NextToken ();

private:
    char
    advance ();

    char
    peek (int relPos = 0) const;
};
$
$}
```

Лексер должен уметь читать символ из исходников. Иногда нужно прочитать следующий или предыдущий символ.
Можно просто писать `_source[_pos + offset]`, но это громоздко и опасно --- можно случайно выйти за пределы
`_source`. Поэтому в лексере будет метод `peek(int relPos = 0)` (`relPos` --- смещение относительно `_pos`),
который решает обе проблемы: уменьшает размер кода и безопасно обрабатывает ситуацию, когда
`_pos + relPos` выходит за пределы `_source` (возвращает `\0`).

Метод `advance` нужен для того, чтобы пропустить текущий символ и **вернуть его же**. Это читаемый шорткат
для записи `++_pos` (пропуск символа).

```cpp
// src/lib/lexer/lexer.cpp

$#include "pebble/lexer/lexer.h"
$#include "pebble/basic/pos.h"
$#include "pebble/diagnostic/codes.h"
$#include "pebble/lexer/keywords.h"
$#include <cctype>
$
$namespace pebble {
$
#define pos(start) pebble::basic::Pos ((start), (_fileId))
#define span(start, end) pebble::diagnostic::Span (pos ((start)), pos ((end)))
#define tok(kind, val, span) pebble::Token ((kind), (val), (span))
#define tok2(kind, val, span) pebble::Token (pebble::TokenKind::kind, (val), (span))

Token
Lexer::NextToken () {
    if (peek () == '\0') {
        return tok2 (Eof, "", span (_pos, _pos));
    }
}

char
Lexer::advance () {
    auto c = peek ();
    ++_pos;
    return c;
}

char
Lexer::peek (int relPos) const {
    if (_pos + relPos < 0 || _pos + relPos >= _source.size ()) {
        return '\0';
    }
    return _source[_pos + relPos];
}

#undef tok2
#undef tok
#undef span
#undef pos
$
$}
```

> Макросы `pos`, `tok`, `tok2` и `span` --- чистый сахар. `pos` генерирует `basic::Pos` из двух чисел,
> `tok` --- создание токена, а `tok2` --- создание токена с **константным `TokenKind`**. `span` ---
> генерация `diagnostic::Span` из двух чисел: начала и конца.

`NextToken` --- тот самый метод, который выдает по одному токену по просьбе синтаксического анализатора.
Он проверяет текущий символ и определяет, как его токенизировать. Пока что `NextToken` возвращает только
`Eof`.

## Тестовый запуск

```cpp
// src/main.cpp

$#include "pebble/basic/loc.h"
$#include "pebble/basic/pos.h"
$#include "pebble/basic/source_mgr.h"
$#include "pebble/diagnostic/colors.h"
$#include "pebble/diagnostic/engine.h"
$#include "pebble/diagnostic/span.h"
$#include "pebble/lexer/lexer.h"
$#include <cstring>
$#include <format>
$#include <fstream>
$#include <sstream>
$#include <vector>

using namespace pebble;

int
main (int argc, char **argv) {
    EnableVirtualTerminalProcessing ();
    basic::SourceMgr             mgr;
    diagnostic::DiagnosticEngine diag (mgr);

    std::string content;
    auto        fileId = mgr.AddFile ("test.pebble", content);
    Lexer       lex (diag, fileId);
    while (true) {
        Token tok = lex.NextToken ();
        if (tok.Kind == TokenKind::Eof) {
            break;
        }
    }

    diag.Render ();
    return static_cast<int> (!diag.HasErrors ());
}
```
