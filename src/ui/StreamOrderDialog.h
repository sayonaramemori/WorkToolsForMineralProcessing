#pragma once

#include <QStringList>
#include <QVector>
#include <functional>
#include <optional>

class QWidget;

namespace afs {

struct StreamOrderEntry { QString streamId; QString displayName; };

// Receives display values only. Filtering eligibility and committing the
// returned order remain responsibilities of the calling application command.
std::optional<QStringList> editStreamOrderDialog(
    QWidget* parent, const QVector<StreamOrderEntry>& ordered,
    const std::function<QVector<StreamOrderEntry>()>& flowOrder);

} // namespace afs
