#pragma once
#include <QHash>
#include <QStringList>

class QGraphicsItem;
namespace afs {
// Non-owning, short-lived navigation index. Rebuild from the current scene;
// never retain across scene edits, imports or undo restoration.
class CanvasItemIndex {
public:
    void insert(const QString& streamId, QGraphicsItem* item) {
        m_byStream.insert(streamId, item);
        m_byItem[item].append(streamId);
    }
    QGraphicsItem* itemForStream(const QString& streamId) const { return m_byStream.value(streamId); }
    QStringList streamsForItem(const QGraphicsItem* item) const { return m_byItem.value(item); }
private:
    QHash<QString, QGraphicsItem*> m_byStream;
    QHash<const QGraphicsItem*, QStringList> m_byItem;
};
} // namespace afs
