#include "SemanticWindow.h"

#include "SemanticAnalysis.h"

#include <QFont>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

SemanticWindow::SemanticWindow(
    const SemanticAnalysisResult& resultado,
    QWidget* parent
) : QDialog(parent) {
    setWindowTitle("Resultados del análisis semántico");
    resize(950, 650);

    auto* layout = new QVBoxLayout(this);
    auto* estado = new QLabel(
        resultado.correcto
            ? "ANÁLISIS SEMÁNTICO: CORRECTO"
            : "ANÁLISIS SEMÁNTICO: CON ERRORES",
        this
    );
    estado->setStyleSheet(
        resultado.correcto
            ? "font-weight: bold; color: #187a2f;"
            : "font-weight: bold; color: #b3261e;"
    );
    layout->addWidget(estado);

    auto* pestañas = new QTabWidget(this);
    pestañas->addTab(
        crearPanel(QString::fromStdString(
            SemanticAnalysis::tablaSimbolos(resultado.estado)
        )),
        "Tabla de símbolos"
    );
    pestañas->addTab(
        crearPanel(QString::fromStdString(
            SemanticAnalysis::pilas(resultado.estado)
        )),
        "Pilas"
    );
    pestañas->addTab(
        crearPanel(QString::fromStdString(
            SemanticAnalysis::cuadruplos(resultado.estado)
        )),
        "Cuádruplos"
    );

    QString errores;
    if (resultado.errores.empty()) {
        errores = "No se encontraron errores semánticos.";
    } else {
        for (const std::string& error : resultado.errores) {
            errores += QString::fromStdString(error) + '\n';
        }
    }
    pestañas->addTab(crearPanel(errores), "Errores semánticos");
    layout->addWidget(pestañas);
}

QTextEdit* SemanticWindow::crearPanel(const QString& texto) {
    auto* panel = new QTextEdit(this);
    panel->setReadOnly(true);
    panel->setFont(QFont("Courier New", 10));
    panel->setPlainText(texto);
    return panel;
}
