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
 * - constantes;
 * - cuadruplos;
 * - pilas de trabajo (cinco columnas verticales con historial completo);
 * - errores semanticos.
 */

#include <QDialog>
#include <QString>

class QTabWidget;
class QTextEdit;
class QTableWidget;
class QWidget;
class QCheckBox;

struct SemanticAnalysisResult;

class SemanticWindow : public QDialog {
    Q_OBJECT

public:
    explicit SemanticWindow(
        const SemanticAnalysisResult& resultado,
        QWidget* parent = nullptr
    );

private slots:
    void onToggleModoNumerico(bool checked);

private:
    QTextEdit* crearPanel(const QString& texto);

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

    void reconstruirColumnasPilas(bool modoNumerico);

    const SemanticAnalysisResult& m_resultado;
    QWidget* m_contenedorColumnasPilas = nullptr;
    QCheckBox* m_chkModoNumerico = nullptr;
};

#endif // SEMANTIC_WINDOW_H
