#pragma once
#include "editor/CanvasProperties.h"
#include <QDockWidget>
class QLabel;
class QDoubleSpinBox;
class QVBoxLayout;
class QWidget;
namespace afs {
class ObjectPropertiesDock final : public QDockWidget {
    Q_OBJECT
public:
    explicit ObjectPropertiesDock(QWidget* parent = nullptr);
    void setState(const ObjectPropertyState& state, const QString& productName);
signals:
    void numericEdited(const QString& objectKey, const QString& key, double value);
    void nameEdited(const QString& objectKey, const QString& name);
    void branchSelected(const QString& objectKey, const QString& branch);
    void resetRequested(const QString& objectKey);
private:
    QWidget* m_form{nullptr};
    QVBoxLayout* m_layout;
    QDoubleSpinBox* m_step;
    ObjectPropertyState m_state;
    quint64 m_revision{0};
    QString m_objectKey;
};
}
