#include "ui/ObjectPropertiesDock.h"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>
namespace afs {
namespace {
constexpr auto stepSetting = "editor/propertyStep";
bool sameForm(const ObjectPropertyState& a, const ObjectPropertyState& b) {
    if (a.objectKey != b.objectKey || a.streamId != b.streamId
        || a.canResetRoute != b.canResetRoute || a.fields.size() != b.fields.size()
        || a.branches.size() != b.branches.size()) return false;
    for (int i = 0; i < a.fields.size(); ++i)
        if (a.fields[i].key != b.fields[i].key || a.fields[i].label != b.fields[i].label) return false;
    for (int i = 0; i < a.branches.size(); ++i)
        if (a.branches[i].id != b.branches[i].id || a.branches[i].label != b.branches[i].label) return false;
    return true;
}
}
ObjectPropertiesDock::ObjectPropertiesDock(QWidget* parent) : QDockWidget("对象属性", parent) {
    setObjectName("objectPropertiesDock"); setMinimumWidth(235);
    auto* scroll = new QScrollArea(this); scroll->setWidgetResizable(true);
    auto* body = new QWidget(scroll); m_layout = new QVBoxLayout(body); m_layout->setContentsMargins(8, 8, 8, 8);
    auto* stepRow = new QFormLayout;
    m_step = new QDoubleSpinBox(body); m_step->setObjectName("propertyStep");
    m_step->setDecimals(2); m_step->setRange(0.01, 5000); m_step->setSingleStep(1);
    const double savedStep = QSettings().value(stepSetting, 10.0).toDouble();
    m_step->setValue(std::isfinite(savedStep) && savedStep >= 0.01 && savedStep <= 5000 ? savedStep : 10.0);
    m_step->setToolTip("属性数值增减按钮和上下方向键的调整幅度，保存为本机偏好。");
    stepRow->addRow("调整步长", m_step); m_layout->addLayout(stepRow);
    connect(m_step, &QDoubleSpinBox::valueChanged, this, [this](double step) {
        QSettings().setValue(stepSetting, step);
        if (m_form) for (auto* editor : m_form->findChildren<QDoubleSpinBox*>()) editor->setSingleStep(step);
    });
    m_layout->addStretch(); scroll->setWidget(body); setWidget(scroll);
}
void ObjectPropertiesDock::setState(const ObjectPropertyState& state, const QString& productName) {
    // Keep editors alive during live updates: rebuilding on every keystroke
    // interrupts typing, cursor position and held increment buttons.
    if (m_form && sameForm(m_state, state)) {
        m_form->findChild<QLabel*>("propertyTitle")->setText(state.title);
        for (const auto& field : state.fields) {
            auto* editor = m_form->findChild<QDoubleSpinBox*>("property_" + field.key);
            const QSignalBlocker blocker(editor);
            if (editor->minimum() != field.minimum || editor->maximum() != field.maximum)
                editor->setRange(field.minimum, field.maximum);
            if (std::abs(editor->value() - field.value) >= 0.005) editor->setValue(field.value);
        }
        if (auto* branches = m_form->findChild<QComboBox*>("propertyBranch")) {
            const QSignalBlocker blocker(branches);
            branches->setCurrentIndex(branches->findData(state.activeBranch));
        }
        if (auto* name = m_form->findChild<QLineEdit*>("propertyProductName"); name && !name->hasFocus())
            name->setText(productName);
        m_state = state;
        return;
    }
    auto* focused = QApplication::focusWidget();
    while (focused && focused->parentWidget() != m_form) focused = focused->parentWidget();
    const QString focusKey = focused && m_objectKey == state.objectKey ? focused->objectName() : QString{};
    const auto revision = ++m_revision;
    if (m_form) { m_layout->removeWidget(m_form); m_form->hide(); m_form->deleteLater(); }
    m_form = new QWidget; auto* form = new QFormLayout(m_form); form->setContentsMargins(0, 0, 0, 0);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto* title = new QLabel(state.title); title->setWordWrap(true); title->setObjectName("propertyTitle"); form->addRow(title);
    const auto object = state.objectKey;
    if (!state.branches.isEmpty()) {
        auto* branches = new QComboBox; branches->setObjectName("propertyBranch");
        for (const auto& branch : state.branches) branches->addItem(branch.label, branch.id);
        branches->setCurrentIndex(branches->findData(state.activeBranch)); form->addRow("当前支路", branches);
        connect(branches, &QComboBox::currentIndexChanged, this, [this, object, revision, branches] {
            if (revision == m_revision) emit branchSelected(object, branches->currentData().toString());
        });
    }
    for (const auto& field : state.fields) {
        auto* editor = new QDoubleSpinBox; editor->setObjectName("property_" + field.key);
        editor->setRange(field.minimum, field.maximum); editor->setDecimals(2); editor->setSingleStep(m_step->value());
        editor->setKeyboardTracking(true); editor->setValue(field.value); form->addRow(field.label, editor);
        connect(editor, &QDoubleSpinBox::valueChanged, this, [this, object, revision, key = field.key](double value) {
            if (revision == m_revision) emit numericEdited(object, key, value);
        });
    }
    if (!state.streamId.isEmpty()) {
        auto* stream = new QLabel(state.streamId); stream->setTextInteractionFlags(Qt::TextSelectableByMouse); form->addRow("物流 ID", stream);
        auto* name = new QLineEdit(productName); name->setObjectName("propertyProductName"); name->setPlaceholderText("未命名产品不参与 Excel 导出");
        form->addRow("产品名称", name);
        connect(name, &QLineEdit::editingFinished, this, [this, object, revision, name] {
            if (revision == m_revision) emit nameEdited(object, name->text());
        });
    }
    if (state.canResetRoute) {
        auto* reset = new QPushButton("恢复自动布线"); reset->setObjectName("propertyResetRoute"); form->addRow(reset);
        connect(reset, &QPushButton::clicked, this, [this, object, revision] {
            if (revision == m_revision) emit resetRequested(object);
        });
    }
    m_layout->insertWidget(1, m_form);
    m_objectKey = state.objectKey;
    m_state = state;
    if (!focusKey.isEmpty())
        if (auto* editor = m_form->findChild<QWidget*>(focusKey)) editor->setFocus(Qt::OtherFocusReason);
}
}
