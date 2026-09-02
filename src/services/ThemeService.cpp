#include "services/ThemeService.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>

namespace afs {

bool ThemeService::loadDarkPreference() {
    return QSettings().value("appearance/darkTheme", false).toBool();
}

void ThemeService::saveDarkPreference(bool dark) {
    QSettings().setValue("appearance/darkTheme", dark);
}

QColor ThemeService::applyApplicationPalette(bool dark) {
    QPalette palette;
    if (dark) {
        palette.setColor(QPalette::Window, QColor("#242628"));
        palette.setColor(QPalette::WindowText, QColor("#f1f3f4"));
        palette.setColor(QPalette::Base, QColor("#1e2022"));
        palette.setColor(QPalette::AlternateBase, QColor("#292c2f"));
        palette.setColor(QPalette::Text, QColor("#f1f3f4"));
        palette.setColor(QPalette::Mid, QColor("#697078"));
        palette.setColor(QPalette::Midlight, QColor("#3d4247"));
        palette.setColor(QPalette::PlaceholderText, QColor("#9aa0a6"));
        palette.setColor(QPalette::Button, QColor("#303336"));
        palette.setColor(QPalette::ButtonText, QColor("#f1f3f4"));
        palette.setColor(QPalette::Highlight, QColor("#4da3ff"));
        palette.setColor(QPalette::HighlightedText, Qt::black);
    } else {
        palette.setColor(QPalette::Window, QColor("#f4f4f4"));
        palette.setColor(QPalette::WindowText, QColor("#202020"));
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor("#f6f8fa"));
        palette.setColor(QPalette::Text, QColor("#202020"));
        palette.setColor(QPalette::Mid, QColor("#8b949e"));
        palette.setColor(QPalette::Midlight, QColor("#d8dee4"));
        palette.setColor(QPalette::PlaceholderText, QColor("#6e7781"));
        palette.setColor(QPalette::Button, QColor("#f4f4f4"));
        palette.setColor(QPalette::ButtonText, QColor("#202020"));
        palette.setColor(QPalette::Highlight, QColor("#1565c0"));
        palette.setColor(QPalette::HighlightedText, Qt::white);
    }
    qApp->setPalette(palette);
    return dark ? QColor("#202224") : QColor("#fafafa");
}

} // namespace afs
