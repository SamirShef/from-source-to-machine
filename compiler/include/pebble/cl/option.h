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
    std::string ValueHint;
    bool        IsSet{};

    Option (const Option &) = default;
    Option (Option &&)      = default;
    Option &
    operator= (const Option &) = default;
    Option &
    operator= (Option &&) = default;

    Option (
        std::string name,
        std::string desc,
        PrefixStyle pref = PrefixStyle::OneOrTwo,
        std::string hint = "")
        : Name (std::move (name)),
          Desc (std::move (desc)),
          Pref (pref),
          ValueHint (std::move (hint)) {
        Registry ().push_back (this);
    }

    virtual ~Option () = default;

    virtual bool
    IsPositional () const {
        return false;
    }

    virtual bool
    IsList () const {
        return false;
    }

    virtual bool
    Parse (std::string_view val, int &idx, int argc, char **argv) = 0;
};

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

    // NOLINTNEXTLINE(hicpp-explicit-conversions)
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

class OptString : public Option {
    std::string _value;

public:
    OptString (
        std::string name,
        std::string desc,
        PrefixStyle prefix = PrefixStyle::OneOrTwo,
        std::string hint   = "<value>")
        : Option (std::move (name), std::move (desc), prefix, std::move (hint)) {}

    const std::string &
    Get () const {
        return _value;
    }

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

}
