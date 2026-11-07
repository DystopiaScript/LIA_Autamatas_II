#include "SemanticWindow.h"

#include "SemanticAnalysis.h"
#include "SemanticParser.h"

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
#include <QCheckBox>

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

// ----------------------------------------------------------------------------
// Formateo de elementos de las 5 pilas
// ----------------------------------------------------------------------------

QString formatearOperador(int cod, bool modoNumerico) {
    QString op = QString::fromStdString(Semantica::codigoAOperador(cod));
    if (modoNumerico) {
        return QString("%1 (%2)").arg(op).arg(cod);
    }
    return op;
}

QString formatearOperando(int cod, const Semantica::EstadoSemantico& estado, bool modoNumerico) {
    if (cod == Semantica::OPERANDO_NULO) {
        return "_";
    }
    // Variable en RAM virtual
    if (cod >= 0 && cod < Semantica::BASE_TEMPORALES) {
        for (const auto& [id, sim] : estado.tablaSimbolos) {
            if (sim.direccionRAM == cod) {
                if (modoNumerico) {
                    return QString("%1 (RAM[%2])").arg(QString::fromStdString(id)).arg(cod);
                }
                return QString::fromStdString(id);
            }
        }
        return QString("VRAM[%1]").arg(cod);
    }
    // Temporal ($T1, $T2...)
    if (cod > Semantica::BASE_TEMPORALES && cod < Semantica::BASE_CONSTANTES) {
        int n = cod - Semantica::BASE_TEMPORALES;
        if (modoNumerico) {
            return QString("t%1 (%2)").arg(n).arg(cod);
        }
        return QString("t%1").arg(n);
    }
    // Constante / Literal
    if (cod > Semantica::BASE_CONSTANTES) {
        auto it = estado.tablaLiterales.find(cod);
        if (it != estado.tablaLiterales.end()) {
            if (modoNumerico) {
                return QString("%1 (%2)").arg(QString::fromStdString(it->second)).arg(cod);
            }
            return QString::fromStdString(it->second);
        }
    }
    return QString::number(cod);
}

QString formatearTipo(int cod, bool modoNumerico) {
    QString t;
    switch (cod) {
        case Semantica::TIPO_NUM_ENTERO:   t = "int"; break;
        case Semantica::TIPO_NUM_REAL:     t = "real"; break;
        case Semantica::TIPO_NUM_CARACTER: t = "char"; break;
        case Semantica::TIPO_NUM_BOOLEANO: t = "bool"; break;
        case Semantica::TIPO_NUM_CADENA:   t = "string"; break;
        default:                           t = "error"; break;
    }
    if (modoNumerico) {
        return QString("%1 (%2)").arg(t).arg(cod);
    }
    return t;
}

QString formatearAvail(int cod, bool modoNumerico) {
    int n = cod > Semantica::BASE_TEMPORALES ? (cod - Semantica::BASE_TEMPORALES) : cod;
    if (modoNumerico) {
        return QString("t%1 (%2)").arg(n).arg(cod);
    }
    return QString("t%1").arg(n);
}

struct TarjetaInfo {
    QString titulo;
    QString accion;
    QStringList items;
    QString pie;
};

QFrame* crearTarjetaWidget(const TarjetaInfo& tarjeta) {
    auto* frame = new QFrame();
    frame->setFrameShape(QFrame::Box);
    frame->setStyleSheet(
        "QFrame {"
        "  background-color: white;"
        "  border: 1px solid #707070;"
        "  border-radius: 0px;"
        "  padding: 4px;"
        "}"
    );

    auto* cardLayout = new QVBoxLayout(frame);
    cardLayout->setContentsMargins(6, 6, 6, 6);
    cardLayout->setSpacing(2);

    auto* lblTitulo = new QLabel(tarjeta.titulo, frame);
    lblTitulo->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: bold; font-size: 9pt; color: #111;"
    );
    cardLayout->addWidget(lblTitulo);

    if (!tarjeta.accion.isEmpty()) {
        auto* lblAccion = new QLabel(tarjeta.accion, frame);
        lblAccion->setStyleSheet(
            "font-family: 'Consolas', 'Courier New', monospace; "
            "font-size: 9pt; color: #222;"
        );
        lblAccion->setWordWrap(true);
        cardLayout->addWidget(lblAccion);
    }

    auto* lblTope = new QLabel("Tope a base:", frame);
    lblTope->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-size: 9pt; color: #333;"
    );
    cardLayout->addWidget(lblTope);

    QString contenidoItems;
    if (tarjeta.items.isEmpty()) {
        contenidoItems = "  (vacia)";
    } else {
        for (int k = 0; k < tarjeta.items.size(); ++k) {
            contenidoItems += QString("  %1. %2\n").arg(k + 1).arg(tarjeta.items[k]);
        }
        if (contenidoItems.endsWith('\n')) {
            contenidoItems.chop(1);
        }
    }

    auto* lblItems = new QLabel(contenidoItems, frame);
    lblItems->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-size: 9pt; color: #000;"
    );
    lblItems->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(lblItems);

    if (!tarjeta.pie.isEmpty()) {
        auto* lblPie = new QLabel(tarjeta.pie, frame);
        lblPie->setStyleSheet(
            "font-family: 'Consolas', 'Courier New', monospace; "
            "font-size: 9pt; color: #444;"
        );
        cardLayout->addWidget(lblPie);
    }

    return frame;
}

QWidget* crearColumnaWidget(const QString& titulo, const QList<TarjetaInfo>& tarjetas) {
    auto* contenedor = new QWidget();
    auto* layout = new QVBoxLayout(contenedor);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QLabel(titulo, contenedor);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(
        "background-color: #e5e7eb;"
        "color: #111827;"
        "font-family: 'Consolas', 'Courier New', monospace;"
        "font-weight: bold;"
        "font-size: 10pt;"
        "padding: 6px;"
        "border: 1px solid #9ca3af;"
        "border-bottom: none;"
    );
    layout->addWidget(header);

    auto* scroll = new QScrollArea(contenedor);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea { border: 1px solid #9ca3af; background-color: #f3f4f6; }"
    );

    auto* cardsContainer = new QWidget(scroll);
    auto* cardsLayout = new QVBoxLayout(cardsContainer);
    cardsLayout->setContentsMargins(5, 5, 5, 5);
    cardsLayout->setSpacing(6);

    for (const auto& t : tarjetas) {
        cardsLayout->addWidget(crearTarjetaWidget(t));
    }
    cardsLayout->addStretch();
    scroll->setWidget(cardsContainer);

    layout->addWidget(scroll, 1);
    return contenedor;
}

} // namespace

SemanticWindow::SemanticWindow(
    const SemanticAnalysisResult& resultado,
    QWidget* parent
) : QDialog(parent), m_resultado(resultado) {
    setWindowTitle("Resultados del análisis semántico");
    resize(1450, 900);
    setMinimumSize(1100, 700);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* estado = new QLabel(
        resultado.correcto
            ? "ANÁLISIS SEMÁNTICO: CORRECTO"
            : "ANÁLISIS SEMÁNTICO: CON ERRORES",
        this
    );
    estado->setStyleSheet(
        resultado.correcto
            ? "font-weight: bold; color: #187a2f; font-size: 15px;"
            : "font-weight: bold; color: #b3261e; font-size: 15px;"
    );
    layout->addWidget(estado);

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
        crearTablaCuadruplos(resultado),
        "Cuádruplos"
    );
    pestañas->addTab(
        crearTablasPilas(resultado),
        "Historial de Pilas"
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
        "padding: 8px 16px; margin-right: 2px; font-weight: bold; }"
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
        "QCheckBox { font-family: 'Consolas', 'Courier New', monospace; font-size: 9pt; }"
    );
}

void SemanticWindow::onToggleModoNumerico(bool checked) {
    reconstruirColumnasPilas(checked);
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

QTableWidget* SemanticWindow::crearTablaCuadruplos(
    const SemanticAnalysisResult& resultado
) {
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

QWidget* SemanticWindow::crearTablasPilas(
    const SemanticAnalysisResult& resultado
) {
    (void)resultado;
    auto* panel = new QWidget(this);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    // Encabezado superior con estilo del diseño didáctico
    auto* headerLayout = new QHBoxLayout();

    auto* titulosBox = new QVBoxLayout();
    titulosBox->setSpacing(2);

    auto* lblTitulo = new QLabel("PILAS - HISTORIAL AUTOMATICO COMPLETO", panel);
    lblTitulo->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: bold; font-size: 11pt; color: #111827;"
    );
    titulosBox->addWidget(lblTitulo);

    auto* lblSubtitulo = new QLabel("Cinco pilas verticales | Historial completo sin seleccionar pasos", panel);
    lblSubtitulo->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-size: 9pt; color: #4b5563;"
    );
    titulosBox->addWidget(lblSubtitulo);

    headerLayout->addLayout(titulosBox);
    headerLayout->addStretch();

    m_chkModoNumerico = new QCheckBox("Mostrar códigos numéricos internos", panel);
    m_chkModoNumerico->setChecked(false);
    connect(m_chkModoNumerico, &QCheckBox::toggled, this, &SemanticWindow::onToggleModoNumerico);
    headerLayout->addWidget(m_chkModoNumerico);

    panelLayout->addLayout(headerLayout);

    // Contenedor dinámico de las 5 columnas verticales
    m_contenedorColumnasPilas = new QWidget(panel);
    auto* colLayout = new QHBoxLayout(m_contenedorColumnasPilas);
    colLayout->setContentsMargins(0, 0, 0, 0);
    colLayout->setSpacing(6);
    panelLayout->addWidget(m_contenedorColumnasPilas, 1);

    reconstruirColumnasPilas(false);

    return panel;
}

void SemanticWindow::reconstruirColumnasPilas(bool modoNumerico) {
    if (!m_contenedorColumnasPilas) return;

    // Limpiar contenido previo de las columnas
    QLayoutItem* item;
    while ((item = m_contenedorColumnasPilas->layout()->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    auto* layout = qobject_cast<QHBoxLayout*>(m_contenedorColumnasPilas->layout());
    if (!layout) return;

    QList<TarjetaInfo> tarjetasOperadores;
    QList<TarjetaInfo> tarjetasOperandos;
    QList<TarjetaInfo> tarjetasTipos;
    QList<TarjetaInfo> tarjetasSaltos;
    QList<TarjetaInfo> tarjetasAvails;

    // -------------------------------------------------------------------------
    // 1. ESTADO INICIAL PARA LAS 5 PILAS
    // -------------------------------------------------------------------------
    tarjetasOperadores.append({"ESTADO\nINICIAL", "", {}, ""});
    tarjetasOperandos.append({"ESTADO\nINICIAL", "", {}, ""});
    tarjetasTipos.append({"ESTADO\nINICIAL", "", {}, ""});
    tarjetasSaltos.append({"ESTADO\nINICIAL", "", {}, ""});

    // AVAILS inicial: pool didáctico de t1 a t20
    QStringList initAvail;
    std::vector<int> prevAvails;
    for (int i = 1; i <= 20; ++i) {
        int cod = Semantica::BASE_TEMPORALES + i;
        prevAvails.push_back(cod);
        initAvail.append(formatearAvail(cod, modoNumerico));
    }
    // En el vector interno, el tope está al final (t1), así que para inicializar
    // mantenemos [t20, t19, ..., t1] para que el tope sea t1
    std::reverse(prevAvails.begin(), prevAvails.end());

    tarjetasAvails.append({"ESTADO\nINICIAL", "", initAvail, ""});

    std::vector<int> prevOperadores;
    std::vector<int> prevOperandos;
    std::vector<int> prevTipos;
    std::vector<int> prevSaltos;

    int cambiosOperadores = 0;
    int cambiosOperandos = 0;
    int cambiosTipos = 0;
    int cambiosSaltos = 0;
    int cambiosAvails = 0;

    // -------------------------------------------------------------------------
    // 2. RECORRIDO DEL HISTORIAL PASO POR PASO
    // -------------------------------------------------------------------------
    for (const auto& paso : m_resultado.estado.historialPilas) {
        const QString pasoTitulo = QString("PASO %1").arg(paso.paso);
        const QString accionPaso = QString::fromStdString(paso.accion);

        // --- OPERADORES ---
        if (paso.operadoresNum != prevOperadores) {
            cambiosOperadores++;
            TarjetaInfo t;
            t.titulo = pasoTitulo;
            if (paso.operadoresNum.size() > prevOperadores.size()) {
                int cod = paso.operadoresNum.back();
                t.accion = "PUSH\nOperadores: " + formatearOperador(cod, modoNumerico);
            } else if (paso.operadoresNum.size() < prevOperadores.size()) {
                int cod = prevOperadores.empty() ? 0 : prevOperadores.back();
                if (cod == Semantica::OP_NUM_PAREN_ABRE) {
                    t.accion = "POP fondo\nfalso '('";
                } else {
                    t.accion = "POP\nOperadores: " + formatearOperador(cod, modoNumerico);
                }
            } else {
                t.accion = accionPaso;
            }
            for (auto it = paso.operadoresNum.rbegin(); it != paso.operadoresNum.rend(); ++it) {
                t.items.append(formatearOperador(*it, modoNumerico));
            }
            tarjetasOperadores.append(t);
            prevOperadores = paso.operadoresNum;
        }

        // --- OPERANDOS ---
        if (paso.operandosNum != prevOperandos) {
            cambiosOperandos++;
            TarjetaInfo t;
            t.titulo = pasoTitulo;
            if (accionPaso.contains("declaracion")) {
                t.accion = accionPaso;
            } else if (paso.operandosNum.size() > prevOperandos.size()) {
                int cod = paso.operandosNum.back();
                t.accion = "PUSH\nOperandos : " + formatearOperando(cod, m_resultado.estado, modoNumerico);
            } else if (paso.operandosNum.size() < prevOperandos.size()) {
                int cod = prevOperandos.empty() ? 0 : prevOperandos.back();
                t.accion = "POP\nOperandos : " + formatearOperando(cod, m_resultado.estado, modoNumerico);
            } else {
                t.accion = accionPaso;
            }
            for (auto it = paso.operandosNum.rbegin(); it != paso.operandosNum.rend(); ++it) {
                t.items.append(formatearOperando(*it, m_resultado.estado, modoNumerico));
            }
            tarjetasOperandos.append(t);
            prevOperandos = paso.operandosNum;
        }

        // --- TIPOS ---
        if (paso.tiposNum != prevTipos) {
            cambiosTipos++;
            TarjetaInfo t;
            t.titulo = pasoTitulo;
            if (accionPaso.contains("destino") || accionPaso.contains("Operandos/Tipos")) {
                t.accion = accionPaso;
            } else if (paso.tiposNum.size() > prevTipos.size()) {
                int cod = paso.tiposNum.back();
                t.accion = "PUSH\nTipo: " + formatearTipo(cod, modoNumerico);
            } else if (paso.tiposNum.size() < prevTipos.size()) {
                int cod = prevTipos.empty() ? 0 : prevTipos.back();
                t.accion = "POP\nTipo: " + formatearTipo(cod, modoNumerico);
            } else {
                t.accion = accionPaso;
            }
            for (auto it = paso.tiposNum.rbegin(); it != paso.tiposNum.rend(); ++it) {
                t.items.append(formatearTipo(*it, modoNumerico));
            }
            tarjetasTipos.append(t);
            prevTipos = paso.tiposNum;
        }

        // --- SALTOS ---
        if (paso.saltos != prevSaltos) {
            cambiosSaltos++;
            TarjetaInfo t;
            t.titulo = pasoTitulo;
            if (paso.saltos.size() > prevSaltos.size()) {
                int salto = paso.saltos.back();
                t.accion = QString("PUSH\nSalto: cuadruplo %1").arg(salto);
            } else if (paso.saltos.size() < prevSaltos.size()) {
                int salto = prevSaltos.empty() ? 0 : prevSaltos.back();
                t.accion = QString("POP\nSalto: completado %1").arg(salto);
            } else {
                t.accion = accionPaso;
            }
            for (auto it = paso.saltos.rbegin(); it != paso.saltos.rend(); ++it) {
                t.items.append(QString("Cuadruplo %1").arg(*it));
            }
            tarjetasSaltos.append(t);
            prevSaltos = paso.saltos;
        }

        // --- AVAILS ---
        if (paso.availNum != prevAvails) {
            cambiosAvails++;
            TarjetaInfo t;
            t.titulo = pasoTitulo;
            if (paso.availNum.size() < prevAvails.size()) {
                int cod = prevAvails.empty() ? 0 : prevAvails.back();
                t.accion = "POP Avails:\nocupar " + formatearAvail(cod, modoNumerico);
            } else if (paso.availNum.size() > prevAvails.size()) {
                int cod = paso.availNum.empty() ? 0 : paso.availNum.back();
                t.accion = "PUSH Avails:\nliberar " + formatearAvail(cod, modoNumerico);
            } else {
                t.accion = accionPaso;
            }
            for (auto it = paso.availNum.rbegin(); it != paso.availNum.rend(); ++it) {
                t.items.append(formatearAvail(*it, modoNumerico));
            }
            tarjetasAvails.append(t);
            prevAvails = paso.availNum;
        }
    }

    // -------------------------------------------------------------------------
    // 3. TARJETA DE ESTADO FINAL PARA CADA PILA
    // -------------------------------------------------------------------------
    auto agregarEstadoFinal = [&](QList<TarjetaInfo>& lista, int cambios, const QStringList& itemsFinales) {
        if (cambios == 0) {
            lista.append({
                "Sin\nmovimientos\nen esta pila.",
                "--- ESTADO\nFINAL ---",
                itemsFinales,
                "Cambios\nregistrados:\n0"
            });
        } else {
            lista.append({
                "--- ESTADO\nFINAL ---",
                "",
                itemsFinales,
                QString("Cambios\nregistrados:\n%1").arg(cambios)
            });
        }
    };

    QStringList opFin, operFin, tipFin, salFin, avFin;
    for (auto it = prevOperadores.rbegin(); it != prevOperadores.rend(); ++it) {
        opFin.append(formatearOperador(*it, modoNumerico));
    }
    for (auto it = prevOperandos.rbegin(); it != prevOperandos.rend(); ++it) {
        operFin.append(formatearOperando(*it, m_resultado.estado, modoNumerico));
    }
    for (auto it = prevTipos.rbegin(); it != prevTipos.rend(); ++it) {
        tipFin.append(formatearTipo(*it, modoNumerico));
    }
    for (auto it = prevSaltos.rbegin(); it != prevSaltos.rend(); ++it) {
        salFin.append(QString("Cuadruplo %1").arg(*it));
    }
    for (auto it = prevAvails.rbegin(); it != prevAvails.rend(); ++it) {
        avFin.append(formatearAvail(*it, modoNumerico));
    }

    agregarEstadoFinal(tarjetasOperadores, cambiosOperadores, opFin);
    agregarEstadoFinal(tarjetasOperandos, cambiosOperandos, operFin);
    agregarEstadoFinal(tarjetasTipos, cambiosTipos, tipFin);
    agregarEstadoFinal(tarjetasSaltos, cambiosSaltos, salFin);
    agregarEstadoFinal(tarjetasAvails, cambiosAvails, avFin);

    // -------------------------------------------------------------------------
    // 4. CREACIÓN DE LAS 5 COLUMNAS VISUALES
    // -------------------------------------------------------------------------
    layout->addWidget(crearColumnaWidget("OPERADORES", tarjetasOperadores), 1);
    layout->addWidget(crearColumnaWidget("OPERANDOS", tarjetasOperandos), 1);
    layout->addWidget(crearColumnaWidget("TIPOS", tarjetasTipos), 1);
    layout->addWidget(crearColumnaWidget("SALTOS", tarjetasSaltos), 1);
    layout->addWidget(crearColumnaWidget("AVAILS", tarjetasAvails), 1);
}
