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
        // CAD-style dark canvas: near-black backgrounds prevent glare while
        // the Text role remains near white, which every flowsheet item uses
        // for its linework and labels.
        palette.setColor(QPalette::Window, QColor("#151a21"));
        palette.setColor(QPalette::WindowText, QColor("#f4f7fb"));
        palette.setColor(QPalette::Base, QColor("#10151c"));
        palette.setColor(QPalette::AlternateBase, QColor("#19212b"));
        palette.setColor(QPalette::Text, QColor("#f4f7fb"));
        palette.setColor(QPalette::BrightText, Qt::white);
        palette.setColor(QPalette::Mid, QColor("#8491a0"));
        palette.setColor(QPalette::Midlight, QColor("#303b47"));
        palette.setColor(QPalette::PlaceholderText, QColor("#aab5c1"));
        palette.setColor(QPalette::Button, QColor("#202832"));
        palette.setColor(QPalette::ButtonText, QColor("#f4f7fb"));
        palette.setColor(QPalette::ToolTipBase, QColor("#202832"));
        palette.setColor(QPalette::ToolTipText, QColor("#f4f7fb"));
        palette.setColor(QPalette::Highlight, QColor("#4da3ff"));
        palette.setColor(QPalette::HighlightedText, QColor("#07111e"));
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
    return dark ? QColor("#0b0f14") : QColor("#fafafa");
}

} // namespace afs
