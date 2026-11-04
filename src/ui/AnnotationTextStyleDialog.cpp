#include "ui/AnnotationTextStyleDialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPalette>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace afs {

std::optional<AnnotationTextSettings> editAnnotationTextStyleDialog(
    QWidget* parent, const AnnotationTextSettings current) {
    // Preserve the existing translation context while moving widget ownership.
    const auto tr = [](const char* text) { return QCoreApplication::translate("MainWindow", text); };
    QDialog dialog(parent);
    dialog.setWindowTitle(tr("标注文字样式"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* pointSize = new QSpinBox(&dialog);
    pointSize->setRange(7, 36);
    pointSize->setSuffix(tr(" pt"));
    pointSize->setValue(current.pointSize);
    form->addRow(tr("字体大小"), pointSize);

    auto* bold = new QCheckBox(tr("加粗"), &dialog);
    bold->setChecked(current.bold);
    form->addRow(tr("字重"), bold);

    auto* followTheme = new QCheckBox(tr("跟随界面主题颜色"), &dialog);
    followTheme->setChecked(!current.color.isValid());
    form->addRow(tr("颜色模式"), followTheme);

    QColor selectedColor = current.color.isValid()
        ? current.color : qApp->palette().color(QPalette::Text);
    auto* colorButton = new QPushButton(&dialog);
    const auto refreshColorButton = [&] {
        colorButton->setText(selectedColor.name(QColor::HexRgb));
        colorButton->setStyleSheet(QString("background:%1; color:%2;")
            .arg(selectedColor.name(), selectedColor.lightness() < 128 ? "white" : "black"));
        colorButton->setEnabled(!followTheme->isChecked());
    };
    refreshColorButton();
    QObject::connect(followTheme, &QCheckBox::toggled, &dialog,
            [&](bool) { refreshColorButton(); });
    QObject::connect(colorButton, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(
            selectedColor, &dialog, tr("选择标注文字颜色"), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { selectedColor = chosen; refreshColorButton(); }
    });
    form->addRow(tr("文字颜色"), colorButton);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    auto settings = current;
    settings.pointSize = pointSize->value();
    settings.bold = bold->isChecked();
    settings.color = followTheme->isChecked() ? QColor() : selectedColor;
    return settings;
}

} // namespace afs
