#include "SemanticWindow.h"

#include "SemanticAnalysis.h"

#include <QFont>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QStringList>
#include <QScrollArea>
#include <QFrame>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

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

void agregarFilaPila(
    QTableWidget* tabla,
    int fila,
    const QString& posicion,
    const QString& contenido
) {
    tabla->insertRow(fila);
    tabla->setItem(fila, 0, celda(posicion));
    tabla->setItem(fila, 1, celda(contenido));
}

} // namespace

SemanticWindow::SemanticWindow(
    const SemanticAnalysisResult& resultado,
    QWidget* parent
) : QDialog(parent) {
    // SemanticWindow no analiza codigo. Recibe una fotografia del estado que
    // Parser ya produjo y la convierte en una vista didactica.
    setWindowTitle("Resultados del análisis semántico");
    resize(1450, 900);
    setMinimumSize(1100, 700);

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
    // El proceso reúne el seguimiento de pilas y los cuadruplos relacionados.
    auto* pestañas = new QTabWidget(this);
    pestañas->addTab(
        crearTablaSimbolos(resultado),
        "Tabla de símbolos"
    );
    pestañas->addTab(
        crearTablaConstantes(resultado),
        "Constantes"
    );
    pestañas->addTab(
        crearTablasPilas(resultado),
        "Proceso semántico"
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

    setStyleSheet(
        "QDialog, QWidget { background-color: #f0f0f0; color: #202020; }"
        "QTabWidget::pane { border: 1px solid #cbd5e1; "
        "background: #f0f0f0; border-radius: 6px; }"
        "QTabBar::tab { background: #e6e6e6; color: #202020; "
        "padding: 9px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #2563eb; color: white; "
        "border-radius: 4px; }"
        "QTableWidget, QTextEdit { background-color: white; color: #202020; "
        "border: 1px solid #a0a0a0; }"
        "QTableWidget::item { color: #202020; }"
        "QHeaderView::section { background-color: #d9d9d9; color: #202020; "
        "font-weight: bold; border: 1px solid #a0a0a0; }"
        "QGroupBox { font-weight: bold; margin-top: 10px; "
        "background: white; border: 1px solid #a0a0a0; "
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
    // La tabla de variables filtra esConstante=false. Las constantes no se
    // repiten aqui: tienen una pestaña propia para distinguir claramente
    // simbolos modificables de simbolos fijos.
    auto* tabla = new QTableWidget(this);
    tabla->setColumnCount(5);
    tabla->setHorizontalHeaderLabels({
        "Identificador",
        "Tipo",
        "RAM virtual",
        "Inicializada",
        "Valor actual"
    });

    int fila = 0;
    for (const auto& entrada : resultado.estado.tablaSimbolos) {
        const auto& simbolo = entrada.second;
        if (simbolo.esConstante) {
            continue;
        }
        tabla->insertRow(fila);
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
            // "Desconocido" significa que el compilador no pudo propagar el
            // valor, no que la celda de memoria virtual contenga texto.
            simbolo.valorConocido
                ? QString::fromStdString(simbolo.valorActual)
                : "Desconocido"
        ));
        ++fila;
    }

    if (fila == 0) {
        tabla->insertRow(0);
        tabla->setItem(0, 0, celda("No hay variables declaradas"));
        tabla->setSpan(0, 0, 1, 5);
    }

    prepararTabla(tabla);
    return tabla;
}

QTableWidget* SemanticWindow::crearTablaConstantes(
    const SemanticAnalysisResult& resultado
) {
    // Una constante sigue siendo un simbolo, pero se presenta aparte porque
    // su valor es fijo y no puede ser destino de asignacion ni de input.
    auto* tabla = new QTableWidget(this);
    tabla->setColumnCount(5);
    tabla->setHorizontalHeaderLabels({
        "Identificador",
        "Tipo",
        "Valor",
        "RAM virtual",
        "Inicializada"
    });

    int fila = 0;
    for (const auto& entrada : resultado.estado.tablaSimbolos) {
        const auto& simbolo = entrada.second;
        if (!simbolo.esConstante) {
            continue;
        }

        tabla->insertRow(fila);
        tabla->setItem(fila, 0, celda(
            QString::fromStdString(simbolo.id)
        ));
        tabla->setItem(fila, 1, celda(
            QString::fromStdString(
                Semantica::tipoToString(simbolo.tipo)
            )
        ));
        tabla->setItem(fila, 2, celda(
            QString::fromStdString(simbolo.valorConstante)
        ));
        tabla->setItem(fila, 3, celda(
            QString("VRAM[%1]").arg(simbolo.direccionRAM)
        ));
        tabla->setItem(fila, 4, celda(
            simbolo.inicializada ? "Sí" : "No"
        ));
        ++fila;
    }

    if (fila == 0) {
        tabla->insertRow(0);
        tabla->setItem(0, 0, celda("No hay constantes declaradas"));
        tabla->setSpan(0, 0, 1, 5);
    }

    prepararTabla(tabla);
    return tabla;
}

QWidget* SemanticWindow::crearTablasPilas(
    const SemanticAnalysisResult& resultado
) {
    auto* contenedor = new QWidget(this);
    auto* layout = new QVBoxLayout(contenedor);
    layout->setSpacing(10);
    auto* explicacion = new QLabel(
        "Cada tarjeta es un paso del análisis. La acción indica qué acaba de "
        "ocurrir y las pilas muestran su estado inmediatamente después. "
        "En cada pila se lee de BASE hacia TOPE. Verde = entrada (PUSH), "
        "rojo = salida (POP) y azul = reducción o generación de código.",
        contenedor
    );
    explicacion->setWordWrap(true);
    layout->addWidget(explicacion);

    auto* scroll = new QScrollArea(contenedor);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* pasos = new QWidget(scroll);
    auto* pasosLayout = new QVBoxLayout(pasos);
    pasosLayout->setContentsMargins(8, 8, 8, 8);
    pasosLayout->setSpacing(8);

    const auto textoPila = [](const QString& titulo,
                              const QStringList& elementos) {
        QString texto = titulo + ": BASE";
        if (elementos.isEmpty()) {
            texto += " → vacía";
        } else {
            for (const QString& elemento : elementos) {
                texto += " → " + elemento;
            }
        }
        return texto + " → TOPE";
    };

    for (const auto& estado : resultado.estado.historialPilas) {
        QStringList operandos;
        for (const auto& valor : estado.operandos) {
            operandos << QString::fromStdString(valor);
        }
        QStringList tipos;
        for (const auto& valor : estado.tipos) {
            tipos << QString::fromStdString(valor);
        }
        QStringList operadores;
        for (const auto& valor : estado.operadores) {
            operadores << QString::fromStdString(valor);
        }
        QStringList avail;
        for (const auto& valor : estado.avail) {
            avail << QString::fromStdString(valor);
        }
        QStringList saltos;
        for (const int valor : estado.saltos) {
            saltos << QString::number(valor);
        }
        const QString accion = QString::fromStdString(estado.accion);
        auto* fila = new QFrame(pasos);
        fila->setFrameShape(QFrame::StyledPanel);
        fila->setStyleSheet(
            "QFrame { background-color: white; "
            "border: 1px solid #b0b0b0; }"
        );
        auto* filaLayout = new QHBoxLayout(fila);
        filaLayout->setContentsMargins(12, 8, 12, 8);
        filaLayout->setSpacing(16);

        auto* paso = new QLabel(
            QString("PASO %1").arg(estado.paso), fila
        );
        paso->setStyleSheet(
            "QLabel { color: #555555; font-weight: bold; }"
        );
        filaLayout->addWidget(paso);

        auto* movimiento = new QLabel(fila);
        QString movimientoTexto = accion;
        QString movimientoColor = "#dbeafe";
        if (accion.startsWith("PUSH")) {
            movimientoTexto = "ENTRA ↑ " + accion.section(':', 1).trimmed();
            movimientoColor = "#d9f2df";
        } else if (accion.startsWith("POP")) {
            movimientoTexto = "SALE ↓ " + accion.section(':', 1).trimmed();
            movimientoColor = "#f8d7da";
        } else if (accion.startsWith("REDUCCION")) {
            movimientoTexto = "REDUCE → " + accion.mid(10).trimmed();
        }
        movimiento->setText(movimientoTexto);
        movimiento->setMinimumWidth(190);
        movimiento->setAlignment(Qt::AlignCenter);
        if (accion.startsWith("POP")) {
            QFont fuenteMovimiento = movimiento->font();
            fuenteMovimiento.setStrikeOut(true);
            movimiento->setFont(fuenteMovimiento);
        }
        movimiento->setStyleSheet(
            QString(
                "QLabel { background-color: %1; color: #202020; "
                "font-weight: bold; padding: 7px; "
                "border: 1px solid #999999; }"
            ).arg(movimientoColor)
        );
        filaLayout->addWidget(movimiento);

        auto* estadoPilas = new QLabel(
            textoPila("PilaO", operandos) + "     " +
            textoPila("PTipos", tipos) + "     " +
            textoPila("POper", operadores) + "     " +
            textoPila("AVAIL", avail) + "     " +
            textoPila("PSaltos", saltos),
            fila
        );
        estadoPilas->setWordWrap(false);
        estadoPilas->setTextInteractionFlags(Qt::TextSelectableByMouse);
        estadoPilas->setStyleSheet(
            "QLabel { color: #202020; font-family: 'Courier New'; "
            "font-size: 10pt; padding: 4px; }"
        );
        filaLayout->addWidget(estadoPilas, 1);
        pasosLayout->addWidget(fila);
    }

    if (resultado.estado.historialPilas.empty()) {
        pasosLayout->addWidget(new QLabel(
            "Sin movimientos semánticos registrados.", pasos
        ));
    }
    pasosLayout->addStretch();
    scroll->setWidget(pasos);
    layout->addWidget(scroll, 1);

    auto* grupoCuadruplos = new QGroupBox(
        "Cuádruplos generados durante el proceso",
        contenedor
    );
    auto* layoutCuadruplos = new QVBoxLayout(grupoCuadruplos);
    layoutCuadruplos->addWidget(crearTablaCuadruplos(resultado));
    layout->addWidget(grupoCuadruplos);

    return contenedor;
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
