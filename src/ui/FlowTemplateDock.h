#pragma once
#include "services/FlowTemplateStore.h"
#include <QDockWidget>
class QListWidget;
class QLabel;
class QGraphicsScene;
class QPushButton;
namespace afs {
class FlowTemplatePreview;
class FlowTemplateDock final : public QDockWidget {
    Q_OBJECT
public:
    explicit FlowTemplateDock(QWidget* parent = nullptr);
    void setTemplates(const QVector<FlowTemplateInfo>& templates, const QString& warning = {});
    QString askTemplateName();
    void setPreview(QGraphicsScene* scene, const QString& message = {});
signals:
    void saveSelectionRequested();
    void insertRequested(const QString& id);
    void renameRequested(const QString& id, const QString& name);
    void removeRequested(const QString& id);
    void refreshRequested();
    void previewRequested(const QString& id);
private:
    QListWidget* m_list;
    QLabel* m_status;
    QPushButton* m_insert;
    FlowTemplatePreview* m_preview;
};
}
