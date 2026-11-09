#include "annotations/AnnotationItem.h"
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

namespace afs {

bool AnnotationItem::editUserNote(QWidget* parent, AnnotationRecord& record,
                                  const AnnotationTextSettings& defaults,
                                  const QString& title) {
    QDialog dialog(parent); dialog.setWindowTitle(title);
    auto* layout = new QFormLayout(&dialog);
    auto* text = new QPlainTextEdit(record.text, &dialog);
    text->setPlaceholderText(QObject::tr("输入自定义文字，可使用多行"));
    text->setMinimumSize(360, 130);
    auto* family = new QFontComboBox(&dialog);
    const QString initialFamily = record.noteFontFamily.isEmpty()
        ? QApplication::font().family() : record.noteFontFamily;
    family->setCurrentFont(QFont(initialFamily));
    auto* size = new QSpinBox(&dialog);
    size->setRange(7, 72);
    size->setValue(record.notePointSize > 0 ? record.notePointSize : defaults.pointSize);
    size->setSuffix(QObject::tr(" pt"));
    auto* bold = new QCheckBox(QObject::tr("使用粗体"), &dialog);
    bold->setChecked(record.notePointSize > 0 ? record.noteBold : defaults.bold);
    auto* border = new QCheckBox(QObject::tr("显示边框和背景"), &dialog);
    border->setChecked(record.noteBorderVisible);
    auto* followColor = new QCheckBox(QObject::tr("跟随全局标注颜色"), &dialog);
    followColor->setChecked(!record.noteColor.isValid());
    QColor selectedColor = record.noteColor.isValid()
        ? record.noteColor
        : (defaults.color.isValid() ? defaults.color
                                    : QApplication::palette().color(QPalette::Text));
    auto* colorButton = new QPushButton(&dialog);
    const auto refreshColorButton = [&] {
        colorButton->setEnabled(!followColor->isChecked());
        colorButton->setText(followColor->isChecked()
            ? QObject::tr("跟随全局颜色") : selectedColor.name(QColor::HexRgb));
        colorButton->setStyleSheet(followColor->isChecked()
            ? QString{} : QStringLiteral("QPushButton { color: %1; }")
                              .arg(selectedColor.name(QColor::HexRgb)));
    };
    refreshColorButton();
    QObject::connect(followColor, &QCheckBox::toggled, &dialog,
                     [&](bool) { refreshColorButton(); });
    QObject::connect(colorButton, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(
            selectedColor, &dialog, QObject::tr("选择文字颜色"),
            QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { selectedColor = chosen; refreshColorButton(); }
    });
    layout->addRow(QObject::tr("文字内容"), text);
    layout->addRow(QObject::tr("字体"), family);
    layout->addRow(QObject::tr("字号"), size);
    layout->addRow(QString(), bold);
    layout->addRow(QString(), border);
    layout->addRow(QString(), followColor);
    layout->addRow(QObject::tr("文字颜色"), colorButton);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;
    const QString content = text->toPlainText().trimmed().left(500);
    if (content.isEmpty()) return false;
    record.text = content;
    record.noteFontFamily = family->currentFont().family();
    record.notePointSize = size->value();
    record.noteBold = bold->isChecked();
    record.noteBorderVisible = border->isChecked();
    record.noteColor = followColor->isChecked() ? QColor() : selectedColor;
    return true;
}

} // namespace afs
