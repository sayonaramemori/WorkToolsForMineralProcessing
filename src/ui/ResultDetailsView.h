#pragma once

#include <QFrame>

class QLabel;
class QTableWidget;

namespace afs {

class FlowsheetDocument;
namespace topology { class TopologyGraph; }

class ResultDetailsView final : public QFrame {
public:
    explicit ResultDetailsView(FlowsheetDocument& document, QWidget* parent = nullptr);

    void showStream(const QString& streamId);
    void showUnit(const QString& unitId, const topology::TopologyGraph& graph);
    void showPlaceholder();

private:
    FlowsheetDocument& m_document;
    QLabel* m_title;
    QTableWidget* m_table;

    void configureTable(int rows, int columns, const QStringList& headers);
    void setNumericItem(int row, int column, double value);
    void setTextItem(int row, int column, const QString& value);
};

} // namespace afs
