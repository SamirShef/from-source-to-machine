#include "pebble/cl/help_info.h"
#include "pebble/cl/option.h"
#include "pebble/cl/registry.h"
#include <cstddef>
#include <iostream>
#include <string>

namespace pebble::cl {

static std::size_t IndentLvl = 0;

void
PrintOptions ();

std::size_t
CalculateOptionLen (const Option *opt);

std::size_t
CalculatePrefixLen (PrefixStyle pref);

void
PrintIndent ();

void
PrintDash (PrefixStyle pref);

void
PrintHelpInfo (char *programName) {
    std::cout << "USAGE: " << programName;
    if (!Registry ().empty ()) {
        std::cout << " [options]";
    }
    std::cout << '\n';

    if (!Registry ().empty ()) {
        PrintOptions ();
    }
}

void
PrintOptions () {
    constexpr std::size_t PADDING = 4;
    std::size_t           maxLen{};
    for (const auto *opt : Registry ()) {
        maxLen = std::max (maxLen, CalculateOptionLen (opt));
    }

    std::cout << "\nOPTIONS:\n\n";
    ++IndentLvl;
    for (auto *opt : Registry ()) {
        if (opt->IsPositional ()) {
            continue;
        }
        auto optLen = CalculateOptionLen (opt);
        PrintIndent ();
        PrintDash (opt->Pref);
        std::cout << opt->Name;
        if (!opt->ValueHint.empty ()) {
            std::cout << "=<" << opt->ValueHint << '>';
        }
        if (!opt->Desc.empty ()) {
            std::cout << std::string (maxLen - optLen + PADDING, ' ');
            std::cout << opt->Desc;
        }
        std::cout << '\n';
    }
    --IndentLvl;
}

std::size_t
CalculateOptionLen (const Option *opt) {
    if (opt->IsPositional ()) {
        return 0;
    }
    std::size_t len = CalculatePrefixLen (opt->Pref) + opt->Name.size ();
    if (!opt->ValueHint.empty ()) {
        len += 3 + opt->ValueHint.size ();
        //     ^ `=` and `<>` characters
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

void
PrintIndent () {
    std::cout << std::string (IndentLvl * 2, ' ');
}

void
PrintDash (PrefixStyle pref) {
    std::cout << std::string (CalculatePrefixLen (pref), '-');
}

}
