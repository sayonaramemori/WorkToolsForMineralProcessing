#pragma once

#include <QSet>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

namespace afs {

class StreamFilterProxyModel final : public QSortFilterProxyModel {
public:
    enum Mode { All, Entered, NeedsInput, Terminal, Feed, Recycle };
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setMode(Mode mode);
    void setInterestedOwners(QSet<QString> ids);

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

private:
    Mode m_mode{All};
    QSet<QString> m_interestedOwners;
};

class NumberDelegate final : public QStyledItemDelegate {
public:
    NumberDelegate(double maximum, QObject* parent);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex&) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

private:
    double m_maximum;
};

class StatusDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

} // namespace afs
