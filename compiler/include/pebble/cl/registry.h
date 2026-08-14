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
