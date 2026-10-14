#include "SemanticWindow.h"

#include "SemanticAnalysis.h"

#include <QFont>
#include <QLabel>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

void prepararTabla(QTableWidget* tabla) {
    tabla->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabla->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabla->setAlternatingRowColors(true);
    tabla->horizontalHeader()->setStretchLastSection(true);
    tabla->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabla->verticalHeader()->setVisible(false);
}

QTableWidgetItem* celda(const QString& texto) {
    auto* item = new QTableWidgetItem(texto);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

} // namespace

SemanticWindow::SemanticWindow(
    const SemanticAnalysisResult& resultado,
    QWidget* parent
) : QDialog(parent) {
    // SemanticWindow no analiza codigo. Recibe una fotografia del estado que
    // Parser ya produjo y la convierte en una vista didactica.
    setWindowTitle("Resultados del análisis semántico");
    resize(1150, 760);
    setMinimumSize(900, 600);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    // Este encabezado funciona como un resumen visual de la fase. La
    // semántica ya fue ejecutada por Parser::Analiza(); esta ventana solamente
    // presenta el estado que el parser entregó.
    auto* estado = new QLabel(
        resultado.correcto
            ? "ANÁLISIS SEMÁNTICO: CORRECTO"
            : "ANÁLISIS SEMÁNTICO: CON ERRORES",
        this
    );
    estado->setStyleSheet(
        resultado.correcto
            ? "font-weight: bold; color: #187a2f; font-size: 16px;"
            : "font-weight: bold; color: #b3261e; font-size: 16px;"
    );
    layout->addWidget(estado);

    // Cada pestaña representa una salida diferente de la fase semantica.
    // Separarlas evita mezclar tabla de simbolos, pilas y cuadruplos.
    auto* pestañas = new QTabWidget(this);
    pestañas->addTab(
        crearTablaSimbolos(resultado),
        "Tabla de símbolos"
    );
    pestañas->addTab(
        crearPanel(QString::fromStdString(
            SemanticAnalysis::pilas(resultado.estado)
        )),
        "Pilas"
    );
    pestañas->addTab(
        crearTablaCuadruplos(resultado),
        "Cuádruplos"
    );
    pestañas->addTab(crearTablasVerdad(), "Tablas de verdad");

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

    setStyleSheet(
        "QDialog { background-color: #f5f7fb; }"
        "QTabWidget::pane { border: 1px solid #cbd5e1; "
        "background: white; border-radius: 6px; }"
        "QTabBar::tab { padding: 9px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #2563eb; color: white; "
        "border-radius: 4px; }"
        "QGroupBox { font-weight: bold; margin-top: 10px; "
        "background: white; border: 1px solid #cbd5e1; "
        "border-radius: 6px; padding: 12px; }"
    );
}

QTextEdit* SemanticWindow::crearPanel(const QString& texto) {
    auto* panel = new QTextEdit(this);
    panel->setReadOnly(true);
    panel->setFont(QFont("Courier New", 10));
    panel->setPlainText(texto);
    return panel;
}

QTableWidget* SemanticWindow::crearTablaSimbolos(
    const SemanticAnalysisResult& resultado
) {
    // La tabla se construye desde el map de simbolos. El orden alfabetico
    // proviene de std::map y no de la interfaz.
    auto* tabla = new QTableWidget(this);
    tabla->setColumnCount(6);
    tabla->setHorizontalHeaderLabels({
        "Identificador",
        "Tipo",
        "RAM virtual",
        "Inicializada",
        "Constante",
        "Valor"
    });

    tabla->setRowCount(
        static_cast<int>(resultado.estado.tablaSimbolos.size())
    );
    int fila = 0;
    for (const auto& entrada : resultado.estado.tablaSimbolos) {
        const auto& simbolo = entrada.second;
        tabla->setItem(fila, 0, celda(
            QString::fromStdString(simbolo.id)
        ));
        tabla->setItem(fila, 1, celda(
            QString::fromStdString(
                Semantica::tipoToString(simbolo.tipo)
            )
        ));
        tabla->setItem(fila, 2, celda(
            QString("VRAM[%1]").arg(simbolo.direccionRAM)
        ));
        tabla->setItem(fila, 3, celda(
            simbolo.inicializada ? "Sí" : "No"
        ));
        tabla->setItem(fila, 4, celda(
            simbolo.esConstante ? "Sí" : "No"
        ));
        tabla->setItem(fila, 5, celda(
            QString::fromStdString(simbolo.valorConstante)
        ));
        ++fila;
    }

    prepararTabla(tabla);
    return tabla;
}

QTableWidget* SemanticWindow::crearTablaCuadruplos(
    const SemanticAnalysisResult& resultado
) {
    // Cada fila es un cuadruplo generado por una reduccion o una asignacion.
    auto* tabla = new QTableWidget(this);
    tabla->setColumnCount(5);
    tabla->setHorizontalHeaderLabels({
        "CONT",
        "COPER",
        "OP1",
        "OP2",
        "RESULTADO"
    });
    tabla->setRowCount(
        static_cast<int>(resultado.estado.cuadruplos.size())
    );

    for (int fila = 0;
         fila < static_cast<int>(resultado.estado.cuadruplos.size());
         ++fila) {
        const auto& cuadruplo = resultado.estado.cuadruplos[fila];
        tabla->setItem(fila, 0, celda(QString::number(fila + 1)));
        tabla->setItem(fila, 1, celda(
            QString::fromStdString(cuadruplo.coper)
        ));
        tabla->setItem(fila, 2, celda(
            QString::fromStdString(cuadruplo.op1)
        ));
        tabla->setItem(fila, 3, celda(
            QString::fromStdString(cuadruplo.op2)
        ));
        tabla->setItem(fila, 4, celda(
            QString::fromStdString(cuadruplo.res)
        ));
    }

    prepararTabla(tabla);
    return tabla;
}

QWidget* SemanticWindow::crearTablasVerdad() {
    // Estas tablas son material didactico fijo: explican las operaciones
    // booleanas que el cubo semantico acepta. No sustituyen la validacion
    // dinamica de tipos realizada por SemanticParser.
    auto* contenedor = new QWidget(this);
    auto* layout = new QVBoxLayout(contenedor);

    auto crearTabla = [this](
        const QString& titulo,
        const QStringList& encabezados,
        const QList<QStringList>& filas
    ) {
        auto* grupo = new QGroupBox(titulo, this);
        auto* layoutGrupo = new QVBoxLayout(grupo);
        auto* tabla = new QTableWidget(grupo);
        tabla->setColumnCount(encabezados.size());
        tabla->setHorizontalHeaderLabels(encabezados);
        tabla->setRowCount(filas.size());
        for (int fila = 0; fila < filas.size(); ++fila) {
            for (int columna = 0;
                 columna < filas[fila].size();
                 ++columna) {
                tabla->setItem(
                    fila,
                    columna,
                    celda(filas[fila][columna])
                );
            }
        }
        prepararTabla(tabla);
        layoutGrupo->addWidget(tabla);
        return grupo;
    };

    // Estas tablas documentan las operaciones booleanas que el cubo
    // semántico acepta. No son resultados de una ejecución concreta: son la
    // referencia didáctica para entender &&, || y !.
    layout->addWidget(crearTabla(
        "AND lógico (&&)",
        {"A", "B", "A && B"},
        {
            {"FALSO", "FALSO", "FALSO"},
            {"FALSO", "VERDADERO", "FALSO"},
            {"VERDADERO", "FALSO", "FALSO"},
            {"VERDADERO", "VERDADERO", "VERDADERO"}
        }
    ));
    layout->addWidget(crearTabla(
        "OR lógico (||)",
        {"A", "B", "A || B"},
        {
            {"FALSO", "FALSO", "FALSO"},
            {"FALSO", "VERDADERO", "VERDADERO"},
            {"VERDADERO", "FALSO", "VERDADERO"},
            {"VERDADERO", "VERDADERO", "VERDADERO"}
        }
    ));
    layout->addWidget(crearTabla(
        "NOT lógico (!)",
        {"A", "!A"},
        {
            {"FALSO", "VERDADERO"},
            {"VERDADERO", "FALSO"}
        }
    ));
    return contenedor;
}
