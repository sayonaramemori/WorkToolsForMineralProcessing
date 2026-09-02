#pragma once

#include <QString>

class QPalette;

namespace afs {

class TerminalProductStyle final {
public:
    static QString styleSheet(const QPalette& palette);
};

} // namespace afs
