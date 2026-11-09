#pragma once
#include "services/FlowGroupSnapshot.h"
#include <QString>
#include <QVector>
#include <optional>
namespace afs {
struct FlowTemplateInfo { QString id, name; int units{0}; };
class FlowTemplateStore final {
public:
    explicit FlowTemplateStore(QString directory = {});
    QVector<FlowTemplateInfo> list(QString* error = nullptr) const;
    QString save(const QString& name, const flow_group::Snapshot& snapshot, QString* error = nullptr) const;
    std::optional<flow_group::Snapshot> load(const QString& id, QString* error = nullptr) const;
    bool rename(const QString& id, const QString& name, QString* error = nullptr) const;
    bool remove(const QString& id, QString* error = nullptr) const;
private:
    QString m_directory;
    QString path(const QString& id) const;
};
}
