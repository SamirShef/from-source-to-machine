# Хранитель исходников

## Введение

Как мы уже выяснили, все открытые исходные файлы будут храниться в специальном объекте —
**Менеджере ресурсов** (`SourceMgr`). Но зачем выделять под это отдельный класс?

В рамках одного файла компилятор создаёт тысячи токенов и узлов AST. Каждый из них хранит лишь лёгкую
структуру `Pos`. Чтобы эта позиция оставалась валидной до самого конца работы компилятора, сам текст файла
должен непрерывно находиться в памяти по одному и тому же адресу.

Компилятор не может просто прочитать файл, нарезать токены и удалить строку с текстом из памяти.
Текст файла необходим на этапах анализа типов, оптимизаций и формирования красивых сообщений об ошибках.

`SourceMgr` решает две ключевые задачи:

1. **Владеет содержимым файлов** на протяжении всего жизненного цикла компилятора. Благодаря этому лексер
и парсер могут дёшево использовать `std::string_view` для имён и токенов без копирования строк.
2. **Предоставляет быстрое API** для перевода байтового смещения (`Pos`) в человекочитаемую строку и столбец
(`Loc`), а также для получения конкретных строк кода.

## Как устроен быстрый поиск координат?

Самый простой вариант перевода байтового смещения (`Pos.Start`) в строку и столбец (`Loc`) — пройтись циклом
от начала файла до нужного байта, считая символы перевода строки `\n`. Но такой алгоритм имеет линейную
сложность `O(N)`, где `N` — байтовое смещение. На больших файлах при регулярном выводе ошибок это станет
узким местом.

Чтобы ускорить поиск до `O(log N)`, при загрузке файла `SourceMgr` единожды сканирует его текст, находит все
символы `\n` и сохраняет байтовые смещения начал всех строк в специальный массив `LineStarts`.

Например, для текста `"hello\nworld"` массив `LineStarts` будет содержать:

* `LineStarts[0] = 0` (1-я строка начинается с 0-го байта)
* `LineStarts[1] = 6` (2-я строка начинается с 6-го байта)

Теперь, зная любое смещение байта, мы можем найти номер строки с помощью **бинарного поиска** по массиву
`LineStarts`.

## Реализация

Начнём со структуры, описывающей один загруженный файл:

```cpp
// include/pebble/basic/source_mgr.h

#pragma once
#include "pebble/basic/loc.h"
#include "pebble/basic/pos.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace pebble::basic {

struct File {
    std::uint32_t              Id;         // Уникальный идентификатор файла
    std::string                Name;       // Имя файла или путь к нему
    std::string                Content;    // Полный текст файла
    std::vector<std::uint32_t> LineStarts; // Байтовые смещения начал всех строк
};

}
```

Теперь напишем сам `SourceMgr`, умеющий регистрировать файлы и вычислять координаты:

```cpp
// include/pebble/basic/source_mgr.h

#pragma once
#include "pebble/basic/loc.h"
#include "pebble/basic/pos.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace pebble::basic {

class SourceMgr {
    std::vector<File> _files;

public:
    std::uint32_t
    AddFile (const std::string &name, const std::string &content) {
        auto                       id = static_cast<std::uint32_t> (_files.size ());
        std::vector<std::uint32_t> lineStarts{ 0 }; // Первая строка всегда начинается с 0-го байта

        for (std::size_t i = 0; i < content.size (); ++i) {
            if (content[i] == '\n') {
                lineStarts.push_back (static_cast<std::uint32_t> (i + 1));
            }
        }

        _files.push_back (
            { .Id         = id,
              .Name       = name,
              .Content    = content,
              .LineStarts = std::move (lineStarts) });

        return id;
    }

    const File &
    GetFile (std::uint32_t id) const {
        return _files[id];
    }

    Loc
    FindLoc (Pos pos) const {
        const auto &file = GetFile (pos.FileId);

        // Ищем первую строку, которая начинается СТРОГО ПОСЛЕ pos.Start
        auto it = std::ranges::upper_bound (file.LineStarts, pos.Start);

        // Индекс найденного элемента в массиве идеально совпадает с 1-based номером нашей строки!
        auto line
            = static_cast<std::uint32_t> (std::distance (file.LineStarts.begin (), it));
        auto lineStartOffset = file.LineStarts[line - 1];
        auto col             = pos.Start - lineStartOffset + 1;

        return { line, col };
    }

    std::string_view
    GetLineContent (std::uint32_t id, std::uint32_t line) const {
        const auto &file = GetFile (id);
        if (line == 0 || line > file.LineStarts.size ()) {
            return "";
        }

        auto start = file.LineStarts[line - 1];
        auto end   = line < file.LineStarts.size () ? file.LineStarts[line] - 1
                                                    : file.Content.size ();

        return { file.Content.data () + start, end - start };
    }
};

}
```

## Как работают ключевые методы?

1. **`AddFile`:** Сохраняет текст файла и формирует вектор `LineStarts`. Первый элемент — всегда `0`.
Каждый раз, когда встречаем `\n`, сохраняем индекс следующего за ним байта (`i + 1`).
2. **`FindLoc`:** Выполняет магию бинарного поиска через `std::ranges::upper_bound`.
   * Функция ищет первый элемент в `LineStarts`, который **строго больше** нашего `pos.Start`.
   * Если байт принадлежит 2-й строке (например, смещение `20` в массиве `[0, 15, 42]`), `upper_bound`
   вернёт итератор на `42` (индекс `2`).
   * Расстояние `std::distance` от начала вектора до этого итератора как раз даст нам точный 1-based номер
   строки — `2`!
   * Зная номер строки, мы получаем смещение её начала (`lineStartOffset`) и лёгкой вычитательной арифметикой
   получаем столбец (`col`).
3. **`GetLineContent`:** Возвращает лёгкий слайс `std::string_view` на конкретную строку файла без выделения
памяти. Конечным байтом строки считается байт прямо перед `\n` следующей строки.
