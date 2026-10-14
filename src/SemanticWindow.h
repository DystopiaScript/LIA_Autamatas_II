#ifndef SEMANTIC_WINDOW_H
#define SEMANTIC_WINDOW_H

#include <QDialog>
#include <QString>

class QTabWidget;
class QTextEdit;

struct SemanticAnalysisResult;

class SemanticWindow : public QDialog {
    Q_OBJECT

public:
    explicit SemanticWindow(
        const SemanticAnalysisResult& resultado,
        QWidget* parent = nullptr
    );

private:
    QTextEdit* crearPanel(const QString& texto);
};

#endif // SEMANTIC_WINDOW_H
