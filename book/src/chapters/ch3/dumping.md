# Всевидящее око

## Введение

"Всевидящее око" --- это специальная часть главы, которая позволяет вывести какой-либо этап компиляции в
консоль (`stdout`). В рамках данного раздела всегда будет создаваться новый флаг `--dump-x`, где `x` ---
то, что мы хотим вывести (например, `--dump-tokens`, `--dump-ast`).

Флаг... Теперь нужно как-то парсить CLI аргументы. Вы можете попробовать сделать так, как умеете. Например,
просто пройтись в цикле по всем аргументам:

```cpp
// src/main.cpp

int
main (int argc, char **argv) {
    bool dumpTokens = false;
    std::string_view targetFile;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--dump-tokens") {
            dumpTokens = true;
        } else {
            targetFile = arg;
        }
    }

    // Запуск компиляции...

    return 0;
}
```

Такой способ имеет место быть и может быть хорош в качестве временной заглужки. Но по мере роста проекта
нам понадобиться всё больше функционала, более простого добавления новых флагов и, что не мало важно, захотим
передавать флаги в другие части компилятора, а не хранить их только в `main`.

Эта книга предлагает вам такой подход: мы сразу напишем **плоскую, базовую реализацию полноценного движка для
парсинга CLI аргументов**, чтобы потом больше не возвращаться к нему (максимум добавить новых флагов).
Этот движок будет покрывать большую часть всех потребностей для учебного компилятора. Он не будет таким
гибким, как хотелось бы, но зато будет понятным для вас. На его основе можно написать более гибкую версию,
но в рамках этой книги не будем себя мучить.

## Виды аргументов

Аргументы делятся на несколько видов:

1. Флаги (простое указание программе включить/выключить какой-то режим работы)
2. Опции-значения (указание значения какому-то параметру)
3. Подкоманды (выполнение команды, в книге не рассматривается)
4. Позиционные (всё остальное)

Флаги и опции-значения начинаются с одного или двух символов `-` (например, `-o`, `--dump-tokens`).
В свою очередь позиционные аргументы, как правило, не начинаются с `-`, поэтому их легко отличить. Но
это не значит, что все позиционные аргументы не могут начинаться с `-`! Обычно позиционные аргументы --- это
пути к файлам. Пусть на вашем компьютере будет файл `-anapa2009.zip`. Вам нужно, например, переместить
этот файл в директорию `travels/`. Чтобы сделать это в POSIX системе нужно выполнить команду
`mv -anapa2009.zip travels/-anapa2009.zip`. Но `mv` выдаст такую ошибку:

```text
mv: неверный ключ — «a»
```

`mv` подумал, что `-a` в `-anapa2009.zip` --- это флаг или опция. Как быть в такой ситуации?

Для решения этой проблемы придумали так называемый стоп флаг (выглядит как `--`). Это специальный флаг, после
которого идут **только позиционные аргументы**. Никаких флагов или опций после `--` быть не может. Теперь,
чтобы переместить файл, нужно добавить в начало нашей команды `--`:
`mv -- -anapa2009.zip travels/-anapa2009.zip`. Наш парсер должен триггериться на `--` и корректно обрабатывать
его.

## Немного покумекаем

Давайте определим, что и как движок будет парсить. Во первых давайте подумаем насчёт количества `-`.
Каждый делает так, как хочет. Обычно `-` ставят для односимвольного шортката/флага, а `--` ставят на длинные
флаги и опции. Но это не формальное правило, а просто соглашение, которое периодически нарушается и каждый
делает так, как хочет. Но в любой программе наверняка будут опции и с `-`, и с `--` в начале. Поэтому давайте
сделаем так, чтобы движок позволял выбирать сколько символов `-` будет в начале. А ещё пусть движок разрешает
ставить для одного и того же флага или опции хоть один, хоть два `-`, например, `-version` и `--version`.

Опции-значения должны как-то передавать значения. Есть два варианта: `--option=value` и `--option value`. В
первом случае `--option=value` --- один аргумент, а `--option value` уже два аргумента, хотя это всё ещё одна
опция и одно значение. Пусть движок разрешает оба варианта по умолчанию.

Парсинг это, конечно, хорошо, но нужно как-то регистрировать и передавать эти опции. Есть два варианта
регистрации: в специальный менеджер и глобально-декларативно. Менеджер --- специальный объект, который
рождается в `main`. В него регистрируются все флаги и опции. Чтобы другие части программы могли получать
значения флагов и опций, нужно передавать этот менеджер по ссылке из `main`.

```cpp
// Псевдокод

int
main () {
    CommandsMgr cmd;
    cmd.RegisterOption ("dump-tokens");
    cmd.RegisterOption ("help");
}
```

Глобально-декларативный
способ заключается в том, что существует специальный статический реестр, а опции и флаги регистрируются
как глобальные переменные и добавляются в этот реестр сами. Такой подход не требует передачи реестра по
ссылке, потому что реестр статический и один на всю программу.

```cpp
// Псевдокод
// В .h

inline Option DumpTokens("dump-tokens");
```

Наш движок будет работать в глобально-декларативном режиме.

## Реализация

### Опции

Начнем с самого простого --- как хранить режимы префиксов (символов `-`) у опций и флагов?

```cpp
// include/pebble/cl/prefix_style.h

#pragma once
#include <cstdint>

namespace pebble::cl {

enum class PrefixStyle : std::uint8_t {
    OnlyOne, // Только один `-` (например, `-o`)
    OnlyTwo, // Только два `-` (например, `--dump-tokens`)
    OneOrTwo // И один, и два `-` (например, `-foo`, `--foo`)
};

}
```

Нужно сделать какой-то реестр опций. Как мы уже выяснили, он должен быть статическим. Пусть опция будет
представляться абстрактным классом `Option`, от которого унаследуются разные типы опций (флаги,
опции-значения, позиционные).

```cpp
// include/pebble/cl/registry.h

#pragma once
#include <vector>

namespace pebble::cl {

class Option;

inline std::vector<Option *> &
Registry () {
    static std::vector<Option *> registry;
    return registry;
}

}
```

И реализуем сам `Option`. Любая опция имеет своё имя, своё описание (понадобиться для вывода описания
всех опций по флагу `-h`/`--help`) и `PrefixStyle`.

```cpp
// include/pebble/cl/option.h

#pragma once
#include "pebble/cl/prefix_style.h"
#include "pebble/cl/registry.h"
#include <string>

namespace pebble::cl {

class Option {
public:
    std::string Name;
    std::string Desc;
    PrefixStyle Pref;

    Option (const Option &) = default;
    Option (Option &&)      = default;
    Option &
    operator= (const Option &) = default;
    Option &
    operator= (Option &&) = default;

    Option (
        std::string name,
        std::string desc,
        // Пусть по умолчанию движок разрешает писать опцию
        // с префиксом `-` или `--`
        PrefixStyle pref = PrefixStyle::OneOrTwo)
        : Name (std::move (name)),
          Desc (std::move (desc)),
          Pref (pref) {
        Registry ().push_back (this);
    }

    virtual ~Option () = default;
};

}
```

Опция может просто существовать, а может
**быть установлена** (прописана в аргументах при старте программы). Иногда бывает нужным отследить установили
ли мы опцию или нет.

```cpp
// include/pebble/cl/option.h

// ...

class Option {
    // ...
    PrefixStyle Pref;
    bool IsSet{};

    // ...
};

// ...
```

Опции-значения, как не странно, ожидают значение. Когда мы захотим сделать вывод по команде `--help`, нам
нужно будет как-то показать, что опция-значение ожидает значение. Само значение может называться по-разному.
Вот пример вывода:

```text
USAGE: ./build/bin/pebble [options]

OPTIONS:

  --emit=<value>     Type of output for the compiler to emit
```

> `--emit` --- опция-значение для наглядности.

`<value>` нужно как-то хранить. Сделаем для этого поле `ValueHint` в `Option`:

```diff
// include/pebble/cl/option.h

// ...

class Option {
    // ...
+   std::string ValueHint;
    bool        IsSet{};

    Option (
        std::string name,
        std::string desc,
-       PrefixStyle pref = PrefixStyle::OneOrTwo)
+       PrefixStyle pref = PrefixStyle::OneOrTwo,
+       std::string hint = "")
        : Name (std::move (name)),
          Desc (std::move (desc)),
-         Pref (pref) {
+         Pref (pref),
+         ValueHint (std::move (hint)) {
        Registry ().push_back (this);
    }

    // ...
};

// ...
```

Если опция не подразумевает хранение значения (это флаг или позиционная опция), то `ValueHint` остаётся
пустым.

Так как `Option` --- базовый класс, то нужно как-то различать какой потомок сохранён. По-быстрому накидаем
пару виртуальных методов:

```cpp
// include/pebble/cl/option.h

// ...

class Option {
    // ...

    virtual ~Option () = default;

    virtual bool
    IsPositional () const {
        return false;
    }
};

// ...
```

Производные классы-опции переопределят этот метод, чтобы парсер понимал какая опция перед ним.

Каждая опция сама знает как себя парсить, поэтому можно создать виртуальный метод, который проверяет, можно
ли распарсить опцию из строки. Так как опции могут быть ещё и опциями-значениями, то в этот метод будеe
передаваться ещё и значение, если оно есть в аргументе (написано через `=`). Также этот метод может
двигать курсор парсера (`idx`), если ему встретится запись `--option value`, где `--option` ---
опция-значение (она съест `value` и парсер не должен будет её анализировать).

```cpp
// include/pebble/cl/option.h

// ...

class Option {
    // ...

    virtual bool
    IsPositional () const {
        return false;
    }

    virtual bool
    Parse (std::string_view val, int &idx, int argc, char **argv) = 0;
};

// ...
```

Теперь можно шаг за шагом реализовывать все типы опций. Начнем с самого простого --- флаг.

```cpp
// include/pebble/cl/option.h

// ...

class OptBool : public Option {
    bool _value{};

public:
    OptBool (
        std::string name, std::string desc, PrefixStyle prefix = PrefixStyle::OneOrTwo)
        : Option (std::move (name), std::move (desc), prefix) {}

    bool
    Get () const {
        return _value;
    }

    // Чтобы можно было писать `if (MyFlagOpt) {}`
    operator bool () const {
        return _value;
    }

    bool
    Parse (std::string_view, int &, int, char **) override {
        _value = true;
        IsSet  = true;
        return true;
    }
};

// ...
```

Ничего сложного нет --- если вызывается `Parse`, то без вопросов взводим `IsSet` и `_value` в `true`.

Теперь чуть сложнее --- опции-значения.

```cpp
// include/pebble/cl/option.h

// ...

class OptString : public Option {
    std::string _value;

public:
    OptString (
        std::string name,
        std::string desc,
        PrefixStyle prefix = PrefixStyle::OneOrTwo,
        // По умолчанию будет выводить ожидаемое значение как <value>,
        // как в примере с `--emit` выше
        std::string hint   = "value")
        : Option (std::move (name), std::move (desc), prefix, std::move (hint)) {}

    const std::string &
    Get () const {
        return _value;
    }

    // Чтобы можно было писать *MyOpt и получить значение
    const std::string &
    operator* () const {
        return _value;
    }

    bool
    Parse (std::string_view val, int &idx, int argc, char **argv) override {
        IsSet = true;
        if (!val.empty ()) {
            _value = val;
            return true;
        }
        if (idx + 1 < argc) {
            _value = argv[++idx];
            return true;
        }
        return false;
    }
};

// ...
```

`Parse` проверяет оба случая: когда значение передано в `val` (в CLI было записано через `=`) и когда
значение идет следующим аргументом в CLI. Если значение не было передано, значит парсинг не удался и
возвращается `false`. Парсер обработает это и выдаст ошибку.

Теперь позиционные аргументы. Их разделяют на два вида: одиночный и список. Одиночный позиционный аргумент
--- просто аргумент, который юзер пишет в нужном месте в CLI. Список позиционных аргументов в свою очередь
сохраняет все оставшиеся позиционные аргументы. В большинстве случаев нам понадобиться именно список, но
мы реализуем оба варианта. Чтобы различать какой позиционный аргумент одиночный, а какой список, добавим
новый метод в `Option`:

```cpp
// include/pebble/cl/option.h

// ...

class Option {
    // ...

    virtual bool
    IsPositional () const {
        return false;
    }

    virtual bool
    IsList () const {
        return false;
    }
};

// ...
```

Если опция `IsPositional ()` и `IsList ()`, то перед парсером список позиционных опций.

```cpp
// include/pebble/cl/option.h

// ...

class OptPositionalString : public Option {
    std::string _value;

public:
    OptPositionalString (std::string name, std::string desc)
        : Option (std::move (name), std::move (desc), PrefixStyle::OneOrTwo) {}

    bool
    IsPositional () const override {
        return true;
    }

    const std::string &
    Get () const {
        return _value;
    }

    const std::string &
    operator* () const {
        return _value;
    }

    bool
    Parse (std::string_view val, int &, int, char **) override {
        _value = val;
        IsSet  = true;
        return true;
    }
};

class OptPositionalList : public Option {
    std::vector<std::string> _values;

public:
    OptPositionalList (std::string name, std::string desc)
        : Option (std::move (name), std::move (desc), PrefixStyle::OneOrTwo) {}

    bool
    IsPositional () const override {
        return true;
    }

    bool
    IsList () const override {
        return true;
    }

    const std::vector<std::string> &
    Get () const {
        return _values;
    }

    bool
    Parse (std::string_view val, int &, int, char **) override {
        _values.emplace_back (val);
        IsSet = true;
        return true;
    }
};

// ...
```

Теперь мы можем зарегистрировать все опции компилятора в одном файле:

```cpp
// include/pebble/basic/options.h

#pragma once
#include "pebble/cl/option.h"

namespace pebble {

// -h
inline cl::OptBool ShortHelp ("h", "Display available options", cl::PrefixStyle::OnlyOne);

// --help
inline cl::OptBool Help ("help", "Display available options", cl::PrefixStyle::OnlyTwo);

// --dump-tokens
inline cl::OptBool DumpTokens ("dump-tokens", "Print lexer tokens to stdout");

// Все входные файлы
inline cl::OptPositionalList InputFiles ("input files", "Input files");

}
```

### Парсер

Парсер должен работать с `argc` и `argv` --- это самое главное, что нужно ему передать.

```cpp
// include/pebble/cl/parser.h

#pragma once
#include "pebble/cl/prefix_style.h"
#include <string_view>

namespace pebble::cl {

bool
ParseCommandLineOptions (int argc, char **argv);

}
```

Пусть `ParseCommandLineOptions` возвращает `true`, если парсинг удался. В противном случае (любая ошибка
парсинга) возвращает `false`. Если будет ошибка парсинга, то `ParseCommandLineOptions` сам напечатает
текст ошибки. В точке входа парсинг будет вызываться примерно так:

```cpp
// src/main.cpp

// ...

int
main (int argc, char **argv) {
    if (!cl::ParseCommandLineOptions (argc, argv)) {
        return 1;
    }

    // ...
}
```

Перейдём к реализации `ParseCommandLineOptions`.

```cpp
// src/lib/cl/parser.cpp

#include "pebble/cl/parser.h"
#include "pebble/cl/option.h"
#include "pebble/diagnostic/colors.h"
#include <iostream>
#include <vector>

namespace pebble::cl {

bool
ParseCommandLineOptions (int argc, char **argv) {
    return true;
}

}
```

Давайте немного подумаем как должен работать парсер. Самое главное --- парсер должен вызывать
`Option::Parse` с нужными аргументами. Если `Option::Parse` вернет `true`, то всё хорошо. В противном
случае парсинг завершается с ошибкой.

Начнем с самого простого: реализуем простой цикл, который находит стоп флаг (`--`).

```cpp
// src/lib/cl/parser.cpp

// ...

bool
ParseCommandLineOptions (int argc, char **argv) {
    bool stopFlags = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (!stopFlags && arg == "--") {
            stopFlags = true;
            continue;
        }
    }

    return true;
}

// ...
```

Теперь попробуем распарсить опции и флаги. Если аргумент начинается с `-` или `--` И он идет ДО стоп
флага, то это опция или флаг.

```cpp
// src/lib/cl/parser.cpp

// ...

bool
ParseCommandLineOptions (int argc, char **argv) {
    // ...
    for (int i = 1; i < argc; ++i) {
        // ...
        if (!stopFlags && arg.starts_with ('-')) {
            //            ^^^^^^^^^^^^^^^^^^^^^ `-` и `--` начинаются с `-`
            std::string_view namePart = arg;
            std::string_view valPart;

            auto eqPos = arg.find ('=');
            if (eqPos != std::string_view::npos) {
                namePart = arg.substr (0, eqPos);
                valPart  = arg.substr (eqPos + 1);
            }
            // TODO: Сначала разберём кусок кода выше
        }
    }

    return true;
}

// ...
```

Нужно разбить аргумент на имя и значение. Не забываем про вариант `--option=value`, который будет
передан как один цельный аргумент. Если в аргументе есть символ `=`, то мы разбиваем его на имя (до `=`)
и значение (после `=`). Но нужно ещё немного поработать с именем: в него всё ещё включен префикс (`-` или
`--`). Нужно исключить префикс из имени, а затем найти аргумент в реестре. Если его нет --- ошибка.
Пусть функция `CheckPrefix` будет одновременно проверять, валидный ли префикс у опции и определять
длину префикса. Пусть она возвращает `true`, если префикс валидный, и `false` в противном случае.
А длина будет передаваться в аргумент-ссылка.

```cpp
// include/pebble/cl/parser.h

// ...

inline bool
CheckPrefix (std::string_view arg, PrefixStyle style, std::size_t &prefLen) {
    if (arg.starts_with ("--")) {
        prefLen = 2;
        return style == PrefixStyle::OnlyTwo || style == PrefixStyle::OneOrTwo;
    }
    if (arg.starts_with ("-")) {
        prefLen = 1;
        return style == PrefixStyle::OnlyOne || style == PrefixStyle::OneOrTwo;
    }
    prefLen = 0;
    return false;
}

// ...
```

Эта функция пригодится нам в парсере:

```cpp
// src/lib/cl/parser.cpp

// ...

bool
ParseCommandLineOptions (int argc, char **argv) {
    // ...
    for (int i = 1; i < argc; ++i) {
        // ...
        if (!stopFlags && arg.starts_with ('-')) {
            // ...
            auto eqPos = arg.find ('=');
            if (eqPos != std::string_view::npos) {
                namePart = arg.substr (0, eqPos);
                valPart  = arg.substr (eqPos + 1);
            }

            Option *matchedOpt = nullptr;

            // Пытаемся найти опцию в реестре
            for (auto *opt : Registry ()) {
                if (opt->IsPositional ()) {
                    continue;
                }

                // Ищем среди непозиционных опций
                std::size_t expectedPrefLen = 0;
                if (CheckPrefix (namePart, opt->Pref, expectedPrefLen)) {
                    // Префикс валиден
                    if (namePart.substr (expectedPrefLen) == opt->Name) {
                        // Имя совпало -> опция найдена в реестре
                        matchedOpt = opt;
                        break;
                    }
                }
            }

            if (matchedOpt != nullptr) {
                // Опция найдена
                if (!matchedOpt->Parse (valPart, i, argc, argv)) {
                    // Парсинг вернул ошибку
                    std::cerr << color::RED << color::BOLD << "error:" << color::RESET
                              << " option '" << namePart << "' requires a value\n";
                    return false;
                }
            } else {
                // Опция не найдена (ошибка)
                std::cerr << color::RED << color::BOLD << "error:" << color::RESET
                          << " unknown option '" << arg << "'\n";
                return false;
            }
        }
    }
    // ...
}

// ...
```

Таким образом мы смогли реализовать парсинг флагов и опций. Но что делать с позиционными аргументами?
Для начала нужно найти нужную позиционную опцию в реестре. Порядок их декларации в
`include/pebble/cl/options.h` показывает порядок нахождения позиционных опций. Когда одиночные позиционные
опции заканчиваются, парсер должен добавлять новые позиционные опции в `OptPositionalList`, который тоже
нужно найти в реестре.

```diff
// src/lib/cl/parser.h

// ...

bool
ParseCommandLineOptions (int argc, char **argv) {
+   std::vector<Option *> positionalOpts;
+   OptPositionalList    *positionalListOpt = nullptr;
+
+   for (auto *opt : Registry ()) {
+       if (opt->IsPositional ()) {
+           if (opt->IsList ()) {
+               positionalListOpt = static_cast<OptPositionalList *> (opt);
+           } else {
+               positionalOpts.push_back (opt);
+           }
+       }
+   }
+
+   int  posIdx    = 0;
    bool stopFlags = false;
    // ...
}

// ...
```

Как только парсер нашел все позиционные аргументы, мы можем расширять главный цикл парсинга. Парсер должен
помнить, на какой последней позиционной опции из `positionalOpts` он остановился. Для этого и нужен `posIdx`,
который будет инкрементироваться после каждого парсинга позиционной опции из `positionalOpts`. Но как только
`posIdx` выйдет за пределы `positionalOpts`, оставшиеся позиционные опции будут помещаться в
`positionalListOpt` (если он есть).

```diff
// src/lib/cl/parser.h

// ...

bool
ParseCommandLineOptions (int argc, char **argv) {
    // ...

    for (int i = 1; i < argc; ++i) {
        // ...
        if (!stopFlags && arg.starts_with ('-')) {
            // ...
+       } else {
+           if (posIdx < static_cast<int> (positionalOpts.size ())) {
+               positionalOpts[posIdx]->Parse (arg, i, argc, argv);
+               ++posIdx;
+           } else if (positionalListOpt != nullptr) {
+               positionalListOpt->Parse (arg, i, argc, argv);
+           } else {
+               std::cerr << color::RED << color::BOLD << "error:" << color::RESET
+                         << " unexpected positional argument '" << arg << "'\n";
+               return false;
+           }
        }
    }

    // ...
}

// ...
```

Парсер полностью готов и уже может парсить базовые CLI аргументы!

## Помощь

Но парсера нам мало. Не стоит забывать про базовый флаг `--help`, который выводит сводку по всем
флагам/опциям в программе. Так как мы уже заложили фундамент под описание опций, мы легко сможем вывести
их. Самое сложное --- красивое форматирование. Пусть вывод будет отформатирован примерно вот так:

```text
USAGE: ./build/bin/pebble [options]

OPTIONS:

  -h                  Display available options
  --help              Display available options
  --dump-tokens       Print lexer tokens to stdout
  --option=<value>    Test option
```

Принтер должен динамически определить самую длинную опцию, добавить несколько дополнительных символов, чтобы
текст не выглядел как каша, и напечатать описание.

Пусть за вывод сообщения для `--help` будет отвечать функция `PrintHelpInfo`. В самой первой же строке вывода
выводится имя программы (то, как мы вызвали бинарник в терминале). Это имя всегда передается в `argv[0]`,
который нам нужно передать в `PrintHelpInfo`.

```cpp
// include/pebble/cl/help_info.h

#pragma once

namespace pebble::cl {

void
PrintHelpInfo (char *programName);

}
```

Начнем с самого простого: вывод первой строки. Если у программы нет опций, то зачем выводить `[options]`?
Пусть принтер проверяет наличие опций в реестре перед тем, как напечатать `[options]`.

```cpp
// src/lib/cl/help_info.cpp

#include "pebble/cl/help_info.h"
#include "pebble/cl/option.h"
#include "pebble/cl/registry.h"
#include <cstddef>
#include <iostream>
#include <string>

namespace pebble::cl {

void
PrintHelpInfo (char *programName) {
    std::cout << "USAGE: " << programName;
    if (!Registry ().empty ()) {
        std::cout << " [options]";
    }
    std::cout << '\n';
}

}
```

Теперь попробуем выводить сам список опций. Пусть за это будет отвечать функция `PrintOptions`:

```cpp
// src/lib/cl/help_info.cpp

// ...

void
PrintOptions ();

void
PrintHelpInfo (char *programName) {
    // ...

    if (!Registry ().empty ()) {
        PrintOptions ();
    }
}

void
PrintOptions () {
    constexpr std::size_t PADDING = 4;

    std::cout << "\nOPTIONS:\n\n";
    for (auto *opt : Registry ()) {
        if (opt->IsPositional ()) {
            continue;
        }
        std::cout << opt->Name;
        if (!opt->ValueHint.empty ()) {
            std::cout << "=<" << opt->ValueHint << '>';
        }
        if (!opt->Desc.empty ()) {
            std::cout << std::string (PADDING, ' ');
            std::cout << opt->Desc;
        }
        std::cout << '\n';
    }
}

// ...
```

`PADDING` --- это те самые дополнительные символы, чтобы не превращать всё в кашу. `PrintOptions` проходится
по всем опциям в реестре. Он пропускает позиционные опции, оставляя только флаги и опции-значения.
Затем выводит их имя, `=<ValueHint>`, если это опция-значения, и описание, если оно есть. Текущий вывод будет
выглядеть вот так:

```text
USAGE: ./build/bin/pebble [options]

OPTIONS:

h    Display available options
help    Display available options
dump-tokens    Print lexer tokens to stdout
option=<value>    Test option
```

Мы уже очень близки к красивому выводу. Давайте теперь научим принтер выводить префикс опций.
> Если опция имеет префикс `PrefixStyle::OneOrTwo`, то наш принтер будет выводить префикс `--`. Вы можете
> сделать вывод для этого типа префикса по-своему.

Давайте сделаем отдельную функцию `PrintDash` для вывода суффикса опции.

```cpp
// src/lib/cl/help_info.cpp

// ...

void
PrintDash (PrefixStyle pref);

// ...

void
PrintDash (PrefixStyle pref) {
    if (pref == PrefixStyle::OneOrTwo || pref == PrefixStyle::OnlyTwo) {
        std::cout << "--";
    } else {
        std::cout << "-";
    }
}

// ...
```

```diff

void
PrintOptions () {
    // ...
    for (auto *opt : Registry ()) {
        // ...
+       PrintDash (opt->Pref);
        std::cout << opt->Name;
        // ...
    }
}
```

Теперь вывод будет выглядеть чуть лучше:

```text
USAGE: ./build/bin/pebble [options]

OPTIONS:

-h    Display available options
--help    Display available options
--dump-tokens    Print lexer tokens to stdout
--option=<value>    Test option
```

Осталось две вещи: ставить перед опциями отступы и выравнивать сами опции. Начнем с отступов. Пусть
функция `PrintIndent` будет печатать два символа отступа:

```cpp
// src/lib/cl/help_info.cpp

// ...

void
PrintIndent ();

void
PrintIndent () {
    std::cout << "  ";
}

// ...
```

```diff

void
PrintOptions () {
    // ...
    for (auto *opt : Registry ()) {
        // ...
+       PrintIndent ();
        PrintDash (opt->Pref);
        // ...
    }
}
```

Теперь перед префиксом будет печататься два символа отступа, что преобразует вывод до такого:

```text
USAGE: ./build/bin/pebble [options]

OPTIONS:

  -h    Display available options
  --help    Display available options
  --dump-tokens    Print lexer tokens to stdout
  --option=<value>    Test option

```

Остался последний штрих --- выравнивание. Выравнивание, как мы уже выяснили, динамическое. Это значит, что
принтер должен сам найти самую длинную опцию и выравнивать все опции по ней. Математика здесь простая:
нужно лишь знать длину самой большой опции (`maxLen`), длину опции, которую прямо сейчас выводит принтер
(`optLen`) и дополнительные символы (`PADDING`). Как только принтер выведет саму опцию, он должен поставить
пробелы выравнивания + `PADDING` и только потом вывести описание. `PADDING` мы уже выводим, но нужно теперь
вывести столько пробелов, сколько не хватает до выравнивания, то есть `maxLen - optLen`. Теперь прибавляем
к этому `PADDING` и получаем `maxLen - optLen + PADDING` пробелов нужно вывести после опции.

Пусть за просчёт длины опции будет отвечать функция `CalculateOptionLen`. Но в опцию ещё входит и префикс,
который будет считать функция `CalculatePrefixLen`.

```cpp
// src/lib/cl/help_info.cpp

// ...

std::size_t
CalculateOptionLen (const Option *opt);

std::size_t
CalculatePrefixLen (PrefixStyle pref);

// ...

std::size_t
CalculateOptionLen (const Option *opt) {
    if (opt->IsPositional ()) {
        return 0;
    }
    std::size_t len = CalculatePrefixLen (opt->Pref) + opt->Name.size ();
    if (!opt->ValueHint.empty ()) {
        len += 3 + opt->ValueHint.size ();
        //     ^ Символы `=` и `<>`
    }
    return len;
}

std::size_t
CalculatePrefixLen (PrefixStyle pref) {
    if (pref == PrefixStyle::OneOrTwo || pref == PrefixStyle::OnlyTwo) {
        return 2;
    }
    return 1;
}

// ...
```

Тогда в `PrintOptions` будем считать `maxLen` таким образом:

```cpp
// src/lib/cl/help_info.cpp

void
PrintOptions () {
    constexpr std::size_t PADDING = 4;
    std::size_t           maxLen{};
    for (const auto *opt : Registry ()) {
        maxLen = std::max (maxLen, CalculateOptionLen (opt));
    }

    std::cout << "\nOPTIONS:\n\n";
    // ...
}
```

И немного поменяем главный цикл вывода:

```diff
// src/lib/cl/help_info.cpp

void
PrintOptions () {
    for (auto *opt : Registry ()) {
        if (opt->IsPositional ()) {
            continue;
        }
+       auto optLen = CalculateOptionLen (opt);
        // ...
        if (!opt->Desc.empty ()) {
-           std::cout << std::string (PADDING, ' ');
_           std::cout << std::string (maxLen - optLen + PADDING, ' ');
            std::cout << opt->Desc;
        }
        std::cout << '\n';
    }
}
```

И итоговый вывод преобразуется до:

```text
USAGE: ./build/bin/pebble [options]

OPTIONS:

  -h                  Display available options
  --help              Display available options
  --dump-tokens       Print lexer tokens to stdout
  --option=<value>    Test option

```

То есть то, чего мы и добивались. Осталось добавить обработку `--help` в `main`:

```cpp
// src/main.cpp

// ...

int
main (int argc, char **argv) {
    // ...
    if (!cl::ParseCommandLineOptions (argc, argv)) {
        return 1;
    }
    if (Help || ShortHelp) {
        cl::PrintHelpInfo (argv[0]);
        return 0;
    }

    // ...
}
```

## То, ради чего вы все собрались

Мы наконец-то подобрались к главной цели "Всевидящего ока" --- реализации вывода токенов (`--dump-tokens`).
Для начала вспомним, что `TokenKind` --- перечисление, которое просто так в строку перевести нельзя. В
теории можно привести его к числу и вывести как число, но отлаживать это будет неприятно. Поэтому для
начала напишем функцию, которая переводит `TokenKind` в строку (`const char *`):

```cpp
// include/pebble/lexer/token_kind.h

// ...

inline const char *
TokenKindToString (TokenKind kind) {
#define variant(kind)                                                                    \
    case TokenKind::kind:                                                                \
        return #kind;

    switch (kind) {
        variant (Id);
        variant (Bool);
        variant (Char);
        variant (Int8);
        variant (Int16);
        variant (Int32);
        variant (Int64);
        variant (Uint8);
        variant (Uint16);
        variant (Uint32);
        variant (Uint64);
        variant (Float32);
        variant (Float64);
        variant (String);
        variant (Var);
        variant (Const);
        variant (Fn);
        variant (Ret);
        variant (If);
        variant (Else);
        variant (For);
        variant (Break);
        variant (Continue);
        variant (Struct);
        variant (Nil);
        variant (Import);
        variant (Extern);
        variant (BoolLit);
        variant (CharLit);
        variant (NumLit);
        variant (StrLit);
        variant (Semi);
        variant (Comma);
        variant (Eq);
        variant (Amp);
        variant (Pipe);
        variant (AmpAmp);
        variant (PipePipe);
        variant (Plus);
        variant (Minus);
        variant (PlusEq);
        variant (MinusEq);
        variant (AmpEq);
        variant (PipeEq);
        variant (EqEq);
        variant (Unknown);
        variant (Eof);
    }

#undef variant

    // Заглужка, чтобы не возникло UB, если switch не сработает.
    // Можно поставить __builtin_unreachable(), но его
    // не поддерживает MSVC.
    // Можно поставить std::unreachable(), если вы
    // используете C++23.
    abort ();
}
```

Макрос `variant` --- хак, который позволит конвертировать название перечисления в `C-like` строку. Самое
главное --- после добавления нового токена в `TokenKind` не забыть добавить новый токен и в
`TokenKindToString`. Для этого можно воспользоваться `X-macro` или просто включить `-Werror` (Linux/macOS)
или `/WX` (Windows) в качестве опции для компиляции.

Теперь осталось совсем немного. Если `DumpTokens` имеет значение `true` (выставлен), то компилятор
должен вывести весь список токенов в `stdout`. Заодно добавим обработку `InputFiles` --- Пусть
компиляцией отдельных файлов управляет специальная функция `CompileFile`. Она вернет `true`, если компиляция
прошла успешно и без ошибок, и `false` в противном случае.

```cpp
// src/main.cpp

// ...

std::string
LocToString (basic::Loc loc);

bool
CompileFile (
    const std::string &path, basic::SourceMgr &mgr, diagnostic::DiagnosticEngine &diag);

int
main (int argc, char **argv) {
    EnableVirtualTerminalProcessing ();

    if (!cl::ParseCommandLineOptions (argc, argv)) {
        return 1;
    }

    if (Help || ShortHelp) {
        cl::PrintHelpInfo (argv[0]);
        return 0;
    }

    basic::SourceMgr             mgr;
    diagnostic::DiagnosticEngine diag (mgr);
    bool                         ok = true;
    for (const auto &path : InputFiles.Get ()) {
        if (!CompileFile (path, mgr, diag)) {
            ok = false;
        }
    }
    return ok ? 0 : 1;
}

// Удобная функция для конвертации `Loc` в строку
std::string
LocToString (basic::Loc loc) {
    return std::to_string (loc.Line) + ':' + std::to_string (loc.Col);
}

bool
CompileFile (
    const std::string &path, basic::SourceMgr &mgr, diagnostic::DiagnosticEngine &diag) {
    std::cout << "Compilation file " << path << "...\n";
    std::ifstream file (path);
    if (!file.is_open ()) {
        std::cerr << path << ": Error opening file!\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf ();
    auto  fileId = mgr.AddFile (path, buffer.str ());
    Lexer lex (diag, fileId);
    if (DumpTokens) {
        std::cout << "==== TOKENS ====\n";
    }
    while (true) {
        Token tok = lex.NextToken ();
        if (DumpTokens) {
            auto startLoc = mgr.FindLoc (tok.Span.Start);
            auto endLoc   = mgr.FindLoc (tok.Span.End);
            std::cout << std::format ("[{}]", TokenKindToString (tok.Kind)) << " '"
                      << tok.Val << "' (" << LocToString (startLoc) << '-'
                      << LocToString (endLoc) << ")\n";
        }
        if (tok.Kind == TokenKind::Eof) {
            break;
        }
    }

    diag.Render ();
    return !diag.HasErrors ();
}
```

## Проверка

Напишем небольшой тестовый файл `test.pebble`:

```pebble
var x = 10;
```

И попробуем скомпилировать его командой `./build/bin/pebble test.pebble --dump-tokens` (находясь в корне
репозитория):

```text
Compilation file test.pebble...
==== TOKENS ====
[Var] 'var' (1:1-1:4)
[Id] 'x' (1:5-1:6)
[Eq] '=' (1:7-1:8)
[NumLit] '10' (1:9-1:11)
[Semi] ';' (1:11-1:12)
[Eof] '' (2:1-2:1)
```

Думаю, за это уже можно порадоваться!
