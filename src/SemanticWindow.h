#ifndef SEMANTIC_WINDOW_H
#define SEMANTIC_WINDOW_H

/**
 * @file SemanticWindow.h
 * @brief Ventana de consulta del resultado semantico.
 *
 * Esta clase pertenece a la presentacion, no al analisis. El parser ya
 * termino de recorrer el programa cuando se construye la ventana. Por eso
 * sus metodos solamente convierten el EstadoSemantico en controles Qt:
 *
 * - tabla de simbolos;
 * - pilas de trabajo;
 * - cuadruplos;
 * - errores semanticos.
 */

#include <QDialog>
#include <QString>

class QTabWidget;
class QTextEdit;
class QTableWidget;
class QWidget;

struct SemanticAnalysisResult;

class SemanticWindow : public QDialog {
    Q_OBJECT

public:
    explicit SemanticWindow(
        const SemanticAnalysisResult& resultado,
        QWidget* parent = nullptr
    );

private:
    // Panel de texto usado para informacion lineal, como el contenido final
    // de las pilas o la lista de errores.
    QTextEdit* crearPanel(const QString& texto);

    // Tablas estructuradas para datos que se entienden mejor por columnas.
    QTableWidget* crearTablaSimbolos(
        const SemanticAnalysisResult& resultado
    );
    QTableWidget* crearTablaConstantes(
        const SemanticAnalysisResult& resultado
    );
    QWidget* crearTablasPilas(
        const SemanticAnalysisResult& resultado
    );
    QTableWidget* crearTablaCuadruplos(
        const SemanticAnalysisResult& resultado
    );

};

#endif // SEMANTIC_WINDOW_H
