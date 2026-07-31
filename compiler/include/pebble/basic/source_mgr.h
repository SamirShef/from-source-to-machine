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
    std::uint32_t              Id;
    std::string                Name;
    std::string                Content;
    std::vector<std::uint32_t> LineStarts;
};

class SourceMgr {
    std::vector<File> _files;

public:
    std::uint32_t
    AddFile (const std::string &name, const std::string &content) {
        std::uint32_t              id = _files.size ();
        std::vector<std::uint32_t> lineStarts{ 0 };
        for (std::size_t i = 0; i < content.size (); ++i) {
            if (content[i] == '\n') {
                lineStarts.push_back (i + 1);
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
        auto        it   = std::ranges::upper_bound (file.LineStarts, pos.Start);
        uint32_t    line = std::distance (file.LineStarts.begin (), it);
        auto        lineStartOffset = file.LineStarts[line - 1];
        auto        col             = pos.Start - lineStartOffset + 1;
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
