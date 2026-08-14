#include "pebble/cl/parser.h"
#include "pebble/cl/option.h"
#include <iostream>
#include <vector>

namespace pebble::cl {

bool
ParseCommandLineOptions (int argc, char **argv) {
    std::vector<Option *> positionalOpts;
    OptPositionalList    *positionalListOpt = nullptr;

    for (auto *opt : Registry ()) {
        if (opt->IsPositional ()) {
            if (opt->IsList ()) {
                positionalListOpt = dynamic_cast<OptPositionalList *> (opt);
            } else {
                positionalOpts.push_back (opt);
            }
        }
    }

    int  posIdx    = 0;
    bool stopFlags = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (!stopFlags && arg == "--") {
            stopFlags = true;
            continue;
        }

        std::size_t prefLen = 0;
        if (!stopFlags && arg.starts_with ('-')) {
            std::string_view namePart = arg;
            std::string_view valPart;

            auto eqPos = arg.find ('=');
            if (eqPos != std::string_view::npos) {
                namePart = arg.substr (0, eqPos);
                valPart  = arg.substr (eqPos + 1);
            }

            Option *matchedOpt = nullptr;

            for (auto *opt : Registry ()) {
                if (opt->IsPositional ()) {
                    continue;
                }

                size_t expectedPrefLen = 0;
                if (CheckPrefix (namePart, opt->Pref, expectedPrefLen)) {
                    if (namePart.substr (expectedPrefLen) == opt->Name) {
                        matchedOpt = opt;
                        break;
                    }
                }
            }

            if (matchedOpt != nullptr) {
                if (!matchedOpt->Parse (valPart, i, argc, argv)) {
                    std::cerr << "error: option '" << namePart << "' requires a value\n";
                    return false;
                }
            } else {
                std::cerr << "error: unknown option '" << arg << "'\n";
                return false;
            }
        } else {
            if (posIdx < static_cast<int> (positionalOpts.size ())) {
                positionalOpts[posIdx]->Parse (arg, i, argc, argv);
                ++posIdx;
            } else if (positionalListOpt != nullptr) {
                positionalListOpt->Parse (arg, i, argc, argv);
            } else {
                std::cerr << "error: unexpected positional argument '" << arg << "'\n";
                return false;
            }
        }
    }

    return true;
}

}
