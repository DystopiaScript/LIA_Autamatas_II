#ifndef SEMANTIC_PARSER_H
#define SEMANTIC_PARSER_H

/**
 * @file SemanticParser.h
 * @brief Estado y acciones para análisis semántico y código intermedio.
 *
 * Las funciones están separadas en:
 *  - VerificacionSemantica: símbolos, tipos y cubo semántico.
 *  - GeneracionCodigo: PilaO, POper, AVAIL, CONT y cuádruplos.
 *  - ControlFlujo: PSaltos y backpatching.
 *
 * Todavía no se conectan con Parser; este archivo prepara la capa que después
 * será invocada desde las acciones del parser.
 */

#include <cstddef>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace Semantica {

enum class TipoDato {
    ENTERO,
    REAL,
    CARACTER,
    BOOLEANO,
    CADENA,
    ERROR_SEMANTICO
};

struct MetadataArreglo {
    bool esArreglo = false;
    std::size_t cantidadElementos = 0;
    TipoDato tipoElemento = TipoDato::ERROR_SEMANTICO;
};

struct Simbolo {
    std::string id;
    TipoDato tipo = TipoDato::ERROR_SEMANTICO;
    int direccionRAM = -1;
    MetadataArreglo arreglo;
    bool inicializada = false;
};

struct Cuadruplo {
    std::string coper;
    std::string op1;
    std::string op2;
    std::string res;
};

struct EstadoSemantico {
    // Tabla de símbolos.
    std::map<std::string, Simbolo> tablaSimbolos;

    // Pilas definidas por la metodología del material.
    std::stack<std::string> PilaO;
    std::stack<TipoDato> PTipos;
    std::stack<std::string> POper;
    std::stack<int> PSaltos;

    // AVAIL: temporales disponibles para reutilización.
    std::stack<std::string> temporalesDisponibles;
    int siguienteTemporal = 1;

    // CONT: dirección del siguiente cuádruplo. El material inicia en 1.
    int CONT = 1;
    std::vector<Cuadruplo> cuadruplos;

    int siguienteDireccionRAM = 0;

    // Estado auxiliar de declaraciones y asignaciones.
    std::vector<std::string> identificadoresPendientes;
    std::string idDestinoAsignacion;
    TipoDato tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
};

std::string tipoToString(TipoDato tipo);

/**
 * Acciones de VERIFICACIÓN SEMÁNTICA.
 *
 * Estas acciones trabajan principalmente con la tabla de símbolos, PTipos y
 * el cubo semántico. No generan cuádruplos por sí mismas.
 */
namespace VerificacionSemantica {

void registrarSimbolo(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo
);

const Simbolo& buscarSimbolo(
    const EstadoSemantico& estado,
    const std::string& id
);

void apilarTipo(
    EstadoSemantico& estado,
    TipoDato tipo
);

TipoDato CuboSemantico(
    TipoDato t1,
    const std::string& operador,
    TipoDato t2
);

void validarOperacion(
    EstadoSemantico& estado,
    const std::string& operador
);

bool tiposCompatiblesAsignacion(
    TipoDato destino,
    TipoDato fuente
);

void iniciarAsignacion(
    EstadoSemantico& estado,
    const std::string& idDestino
);

void validarAsignacion(
    EstadoSemantico& estado,
    TipoDato tipoExpresion
);

void acumularIdentificadorDeclaracion(
    EstadoSemantico& estado,
    const std::string& id
);

void registrarDeclaracion(
    EstadoSemantico& estado,
    TipoDato tipo
);

} // namespace VerificacionSemantica

/**
 * Acciones de GENERACIÓN DE CÓDIGO INTERMEDIO.
 *
 * Estas acciones trabajan con PilaO, POper, AVAIL y CONT. Algunas se ejecutan
 * en el mismo punto que una verificación de tipos, pero sus responsabilidades
 * son distintas.
 */
namespace GeneracionCodigo {

void apilarOperando(
    EstadoSemantico& estado,
    const std::string& operando
);

void guardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
);

void abrirParentesis(EstadoSemantico& estado);
void cerrarParentesis(EstadoSemantico& estado);

std::string obtenerTemporalAvail(EstadoSemantico& estado);
void liberarTemporalAvail(
    EstadoSemantico& estado,
    const std::string& operando
);

int generarCuadruplo(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& op1,
    const std::string& op2,
    const std::string& resultado
);

void reducirOperacion(
    EstadoSemantico& estado,
    const std::string& operador
);

void generarAsignacion(
    EstadoSemantico& estado,
    const std::string& expresion
);

int precedencia(const std::string& operador);

} // namespace GeneracionCodigo

/**
 * Acciones de GENERACIÓN Y RELLENADO DE SALTOS.
 */
namespace ControlFlujo {

int generarSalto(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& operando
);

void completarSalto(
    EstadoSemantico& estado,
    int indiceCuadruplo,
    int destino
);

} // namespace ControlFlujo

/**
 * Limpia el estado completo antes de iniciar otro análisis.
 */
void reiniciarEstado(EstadoSemantico& estado);

} // namespace Semantica

#endif // SEMANTIC_PARSER_H
