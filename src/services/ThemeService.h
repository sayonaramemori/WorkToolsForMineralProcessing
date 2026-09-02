#pragma once

#include <QColor>

namespace afs {

class ThemeService final {
public:
    ThemeService() = delete;
    static bool loadDarkPreference();
    static void saveDarkPreference(bool dark);
    static QColor applyApplicationPalette(bool dark);
};

} // namespace afs
