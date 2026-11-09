#pragma once
#include <QString>
#include <QStringList>

namespace afs {
struct StreamDescriptor {
    QString streamId;
    QString displayName;
    bool mergeBranch{false};
    bool terminal{false};
    bool feed{false};
    bool recycle{false};
    QStringList ownerIds;
    friend bool operator==(const StreamDescriptor&, const StreamDescriptor&) = default;
};
} // namespace afs
