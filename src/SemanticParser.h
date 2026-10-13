#ifndef SEMANTIC_PARSER_H
#define SEMANTIC_PARSER_H

/**
 * @file SemanticParser.h
 * @brief Estructuras base para el análisis semántico y la generación de
 *        código intermedio mediante cuádruplos.
 *
 * Este archivo contiene únicamente la interfaz y los datos necesarios para
 * la futura integración con Parser. No modifica el flujo actual del análisis
 * léxico ni sintáctico.
 */

#include <cstddef>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace Semantica {

/**
 * @brief Tipos de datos reconocidos por la gramática actual de LIA.
 */
enum class TipoDato {
    ENTERO,
    REAL,
    CARACTER,
    BOOLEANO,
    CADENA,
    ERROR_SEMANTICO
};

/**
 * @brief Metadatos opcionales para una variable tipo arreglo.
 */
struct MetadataArreglo {
    bool esArreglo = false;
    std::size_t cantidadElementos = 0;
    TipoDato tipoElemento = TipoDato::ERROR_SEMANTICO;
};

/**
 * @brief Entrada de la tabla de símbolos.
 */
struct Simbolo {
    std::string id;
    TipoDato tipo = TipoDato::ERROR_SEMANTICO;
    int direccionRAM = -1;
    MetadataArreglo arreglo;
    bool inicializada = false;
};

/**
 * @brief Representación de un cuádruplo [COPER, OP1, OP2, RES].
 */
struct Cuadruplo {
    std::string coper;
    std::string op1;
    std::string op2;
    std::string res;
};

/**
 * @brief Contexto en el que se procesa un identificador.
 *
 * El parser sintáctico actual usa una pila de símbolos, por lo que más
 * adelante este contexto permitirá distinguir un id de una declaración,
 * una asignación o un factor de una expresión.
 */
enum class ContextoSemantico {
    NINGUNO,
    DECLARACION,
    ASIGNACION,
    FACTOR
};

/**
 * @brief Estado semántico reutilizable por Parser.
 *
 * Las estructuras se agrupan en una sola instancia para evitar que distintas
 * partes del parser trabajen accidentalmente con tablas o pilas diferentes.
 */
struct EstadoSemantico {
    std::map<std::string, Simbolo> tablaSimbolos;

    std::stack<std::string> PilaO;
    std::stack<TipoDato> PTipos;
    std::stack<std::string> POper;
    std::stack<int> PSaltos;

    std::vector<Cuadruplo> cuadruplos;

    int siguienteDireccionRAM = 0;
    int siguienteTemporal = 1;
    int CONT = 0;

    // Identificadores pendientes de una declaración:
    // def a, b, c : int;
    std::vector<std::string> identificadoresPendientes;

    // Datos pendientes de la asignación actual:
    // id = EXPR ;
    std::string idDestinoAsignacion;
    TipoDato tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;

    ContextoSemantico contexto = ContextoSemantico::NINGUNO;
};

/**
 * @brief Convierte un tipo semántico a texto.
 */
std::string tipoToString(TipoDato tipo);

/**
 * @brief Acciones semánticas de los círculos del diagrama.
 *
 * La numeración corresponde al esquema teórico de acciones que se insertará
 * en las producciones del diagrama:
 *
 *  - FACT: leer id o constante y colocar su operando y tipo en las pilas.
 *  - EXPR5 / TERM: guardar +, -, *, / y %.
 *  - EXPR2 / EXPR: guardar && y ||.
 *  - EXPR4 / OPREL: guardar y reducir operadores relacionales.
 *  - FACT -> ( EXPR ): controlar la marca de fondo falso "(".
 *  - Al terminar una reducción: consultar el cubo y generar un cuádruplo.
 *
 * Los números 1 a 7 son nombres de acciones de trabajo para la integración;
 * no son producciones adicionales ni código que ya exista en el proyecto.
 *
 * Se declaran por separado para que Parser pueda invocarlas en los puntos
 * exactos donde hace match de terminales o aplica una producción. Sus
 * implementaciones se agregarán posteriormente en SemanticParser.cpp.
 */
void accion1AcumularIdentificador(
    EstadoSemantico& estado,
    const std::string& id
);

void accion1RegistrarDeclaracion(
    EstadoSemantico& estado,
    TipoDato tipo
);

void accion2GuardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
);

void accion3GuardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
);

void accion4ReducirOperador(
    EstadoSemantico& estado
);

void accion5GenerarOperacion(
    EstadoSemantico& estado
);

void accion6AbrirParentesis(
    EstadoSemantico& estado
);

void accion7CerrarParentesis(
    EstadoSemantico& estado
);

/**
 * @brief Acciones auxiliares para factores.
 *
 * Estas acciones corresponden a la lectura semántica de FACT:
 * id, constantes y ( EXPR ).
 */
void apilarIdentificador(
    EstadoSemantico& estado,
    const std::string& id
);

void apilarConstante(
    EstadoSemantico& estado,
    const std::string& valor,
    TipoDato tipo
);

/**
 * @brief Prepara y finaliza una asignación.
 */
void iniciarAsignacion(
    EstadoSemantico& estado,
    const std::string& idDestino
);

void finalizarAsignacion(
    EstadoSemantico& estado
);

/**
 * @brief Reinicia pilas, cuádruplos y contadores para un nuevo análisis.
 */
void reiniciarEstado(EstadoSemantico& estado);

/**
 * @brief Obtiene un temporal nuevo: $T1, $T2, $T3, ...
 */
std::string obtenerTemporalAvail(EstadoSemantico& estado);

/**
 * @brief Valida dos operandos y devuelve el tipo resultante.
 */
TipoDato CuboSemantico(
    TipoDato t1,
    const std::string& operador,
    TipoDato t2
);

/**
 * @brief Verifica si el tipo de una expresión puede asignarse al destino.
 */
bool tiposCompatiblesAsignacion(
    TipoDato destino,
    TipoDato fuente
);

/**
 * @brief Registra un cuádruplo y devuelve su índice dentro del vector.
 */
int generarCuadruplo(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& op1,
    const std::string& op2,
    const std::string& resultado
);

/**
 * @brief Registra una variable nueva en la tabla de símbolos.
 *
 * Debe reportar error si el identificador ya existe.
 */
void registrarSimbolo(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo
);

/**
 * @brief Busca un símbolo declarado.
 *
 * Debe reportar error si el identificador no fue declarado.
 */
const Simbolo& buscarSimbolo(
    const EstadoSemantico& estado,
    const std::string& id
);

/**
 * @brief Devuelve la precedencia del operador.
 */
int precedencia(const std::string& operador);

/**
 * @brief Reduce el operador en el tope de POper y genera un cuádruplo.
 */
void reducirOperador(EstadoSemantico& estado);

/**
 * @brief Recibe un operador respetando la precedencia aritmética y lógica.
 */
void recibirOperador(
    EstadoSemantico& estado,
    const std::string& operador
);

/**
 * @brief Completa el destino de un cuádruplo de salto pendiente.
 */
void completarSalto(
    EstadoSemantico& estado,
    int indiceCuadruplo,
    int destino
);

} // namespace Semantica

#endif // SEMANTIC_PARSER_H
