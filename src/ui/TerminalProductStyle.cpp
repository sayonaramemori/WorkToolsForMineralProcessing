#include "ui/TerminalProductStyle.h"

#include <QColor>
#include <QPalette>

namespace afs {

QString TerminalProductStyle::styleSheet(const QPalette& palette) {
    const auto color = [&palette](QPalette::ColorRole role) {
        return palette.color(QPalette::Active, role).name(QColor::HexArgb);
    };
    return QString(R"(
        #terminalProductPanel { background: %1; }
        #terminalSummary { background: %2; border: 1px solid %5; border-radius: 8px; }
        #terminalSummaryTitle { font-size: 15px; font-weight: 600; color: %3; }
        #terminalSummaryHint, #calculationStatus { color: %4; }
        #terminalProgress { min-width: 64px; padding: 5px 9px; border-radius: 12px;
            background: %6; color: %3; font-weight: 600; }
        #calculateButton { padding: 5px 14px; border-radius: 6px; border: 1px solid %9;
            background: %9; color: %10; font-weight: 600; }
        #calculateButton:disabled { background: %6; color: %4; border-color: %5; }
        #calculationStatus { padding: 0 2px; }
        #resultDetails { background: %2; border: 1px solid %5; border-radius: 8px; }
        #resultDetailTitle { color: %3; font-weight: 600; }
        QTableView#terminalProductTable { background: %2; alternate-background-color: %6;
            color: %3; border: 1px solid %5; border-radius: 8px; padding: 1px; outline: 0;
            selection-background-color: %9; selection-color: %10; }
        QTableView#terminalProductTable::item { padding: 0 10px; border: 0; }
        QTableView#terminalProductTable::item:hover { background: %5; }
        QHeaderView::section { background: %7; color: %8; border: 0;
            border-bottom: 1px solid %4; padding: 7px 8px; font-weight: 600; }
        QTableWidget#resultDetailTable { background: %2; color: %3; border: 0; }
    )").arg(color(QPalette::Window), color(QPalette::Base), color(QPalette::Text),
             color(QPalette::Mid), color(QPalette::Midlight), color(QPalette::AlternateBase),
             color(QPalette::Button), color(QPalette::ButtonText), color(QPalette::Highlight),
             color(QPalette::HighlightedText));
}

} // namespace afs
