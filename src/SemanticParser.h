#ifndef SEMANTIC_PARSER_H
#define SEMANTIC_PARSER_H

/**
 * @file SemanticParser.h
 * @brief API modular para semántica y generación de código intermedio.
 *
 * Los módulos tienen responsabilidades separadas:
 *
 * 1. VerificacionSemantica:
 *    tabla de símbolos, PTipos, cubo semántico y compatibilidad de tipos.
 * 2. Declaraciones:
 *    acumulación de identificadores y registro de variables/constantes.
 * 3. Expresiones:
 *    acciones del diagrama de EXPR, FACT, TERM, relaciones, &&, || y !.
 * 4. EntradaSalida:
 *    validación de destinos y generación de input/output.
 * 5. GeneracionCodigo:
 *    PilaO, POper, AVAIL, CONT y cuádruplos.
 * 6. ControlFlujo:
 *    PSaltos y backpatching para una etapa posterior.
 *
 * Esta capa permanece independiente del driver LL(1) de Parser.
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
    // No es un puntero C++ ni una direccion fisica del proceso. Es la
    // posicion logica que el compilador asigna dentro de la memoria virtual
    // del programa fuente para poder generar y leer cuadruplos.
    int direccionRAM = -1;
    MetadataArreglo arreglo;
    bool inicializada = false;
    bool esConstante = false;
    std::string valorConstante;
};

struct Cuadruplo {
    std::string coper;
    std::string op1;
    std::string op2;
    std::string res;
};

struct EstadoSemantico {
    // ---------------------------------------------------------------------
    // Estado de verificación semántica
    // ---------------------------------------------------------------------
    std::map<std::string, Simbolo> tablaSimbolos;

    // ---------------------------------------------------------------------
    // Pilas de trabajo definidas por la metodología del material
    // ---------------------------------------------------------------------
    std::stack<std::string> PilaO;
    std::stack<TipoDato> PTipos;
    std::stack<std::string> POper;
    std::stack<int> PSaltos;

    // ---------------------------------------------------------------------
    // Generación de código intermedio
    // ---------------------------------------------------------------------
    // AVAIL: temporales disponibles para reutilización.
    std::stack<std::string> temporalesDisponibles;
    int siguienteTemporal = 1;

    // CONT: dirección del siguiente cuádruplo. El material inicia en 1.
    int CONT = 1;
    std::vector<Cuadruplo> cuadruplos;

    // Contador de direcciones logicas de la RAM virtual del programa.
    // Comienza en cero para que la primera variable ocupe VRAM[0].
    int siguienteDireccionRAM = 0;

    // ---------------------------------------------------------------------
    // Estado transitorio de declaraciones y asignaciones
    // ---------------------------------------------------------------------
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

/**
 * Registra una variable simple, asignándole tipo y dirección RAM.
 * Rechaza identificadores vacíos, tipos inválidos y redeclaraciones.
 */
void registrarSimbolo(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo
);

/**
 * Busca un identificador declarado. Si no existe, lanza un error semántico.
 */
const Simbolo& buscarSimbolo(
    const EstadoSemantico& estado,
    const std::string& id
);

/**
 * Apila un tipo válido en PTipos. Debe acompañar a un elemento de PilaO.
 */
void apilarTipo(
    EstadoSemantico& estado,
    TipoDato tipo
);

/**
 * Consulta la compatibilidad de dos tipos para un operador binario.
 * Devuelve ERROR_SEMANTICO cuando la combinación no es válida.
 */
TipoDato CuboSemantico(
    TipoDato t1,
    const std::string& operador,
    TipoDato t2
);

/**
 * Consume dos tipos, consulta el cubo semántico y apila el tipo resultante.
 */
void validarOperacion(
    EstadoSemantico& estado,
    const std::string& operador
);

/**
 * Valida operaciones unarias: '!' para BOOLEANO y 'NEG' para ENTERO/REAL.
 */
void validarOperacionUnaria(
    EstadoSemantico& estado,
    const std::string& operador
);

/**
 * Determina si el tipo fuente puede asignarse al tipo destino.
 */
bool tiposCompatiblesAsignacion(
    TipoDato destino,
    TipoDato fuente
);

/**
 * Inicia el contexto de una asignación y valida su identificador destino.
 */
void iniciarAsignacion(
    EstadoSemantico& estado,
    const std::string& idDestino
);

/**
 * Valida el tipo final de la expresión contra el destino actual.
 */
void validarAsignacion(
    EstadoSemantico& estado,
    TipoDato tipoExpresion
);

/**
 * Conserva un identificador mientras la declaración aún no tiene tipo.
 */
void acumularIdentificadorDeclaracion(
    EstadoSemantico& estado,
    const std::string& id
);

/**
 * Registra todos los identificadores pendientes con el tipo indicado.
 */
void registrarDeclaracion(
    EstadoSemantico& estado,
    TipoDato tipo
);

} // namespace VerificacionSemantica

/**
 * Acciones semánticas de DECLARACIONES.
 *
 * Este módulo no modifica la gramática. Permite que el parser, al reconocer
 * una declaración, acumule identificadores y después los registre como
 * variables o constantes.
 */
namespace Declaraciones {

/** Limpia los identificadores acumulados de una declaración anterior. */
void iniciarDeclaracion(EstadoSemantico& estado);

/** Añade un identificador a la declaración que está siendo construida. */
void acumularIdentificador(
    EstadoSemantico& estado,
    const std::string& id
);

/** Registra como variables todos los identificadores acumulados. */
void declararVariables(
    EstadoSemantico& estado,
    TipoDato tipo
);

/** Registra una constante con su tipo y representación textual de valor. */
void declararConstante(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo,
    const std::string& valor
);

} // namespace Declaraciones

/**
 * Acciones semánticas de EXPRESIONES.
 *
 * Una expresión puede ser aritmética o booleana. Incluye constantes,
 * identificadores, paréntesis, operadores aritméticos, relacionales, AND y
 * OR. El resultado queda en el tope de PilaO y PTipos.
 */
namespace Expresiones {

/** Busca un id, apila su dirección/valor y apila su tipo. */
void apilarIdentificador(
    EstadoSemantico& estado,
    const std::string& id
);

/** Apila una constante literal y su tipo correspondiente. */
void apilarConstante(
    EstadoSemantico& estado,
    const std::string& valor,
    TipoDato tipo
);

/** Guarda un operador binario en POper. */
void recibirOperador(
    EstadoSemantico& estado,
    const std::string& operador
);

/** Guarda '!' o convierte '-' unario en 'NEG' dentro de POper. */
void recibirOperadorUnario(
    EstadoSemantico& estado,
    const std::string& operador
);

/** Inserta la marca de fondo falso '(' en POper. */
void abrirParentesis(EstadoSemantico& estado);

/** Reduce hasta '(' y elimina la marca de fondo falso. */
void cerrarParentesis(EstadoSemantico& estado);

/** Reduce todos los operadores restantes de la expresión actual. */
void finalizarOperacionPendiente(EstadoSemantico& estado);

/** Extrae el resultado y su tipo de las pilas de la expresión. */
std::string obtenerResultado(
    EstadoSemantico& estado
);

/**
 * Finaliza una asignación usando el resultado que está en las pilas.
 *
 * Esta operación no apila '=' ni ':=' en POper. Primero valida el tipo del
 * resultado, después consume exactamente un elemento de PilaO y PTipos y
 * finalmente genera el cuádruplo de asignación.
 */
void finalizarAsignacion(EstadoSemantico& estado);

} // namespace Expresiones

/**
 * Acciones semánticas de entrada y salida.
 *
 * Entrada recibe identificadores; salida recibe el resultado de una
 * expresión. La generación se mantiene separada de la validación.
 */
namespace EntradaSalida {

/** Comprueba que input reciba una variable existente y modificable. */
void validarDestinoEntrada(
    const EstadoSemantico& estado,
    const std::string& id
);

/** Genera el cuádruplo input para una variable. */
int generarEntrada(
    EstadoSemantico& estado,
    const std::string& id
);

/** Genera el cuádruplo output para el resultado de una expresión. */
int generarSalida(
    EstadoSemantico& estado,
    const std::string& resultadoExpresion
);

} // namespace EntradaSalida

/**
 * Acciones de GENERACIÓN DE CÓDIGO INTERMEDIO.
 *
 * Estas acciones trabajan con PilaO, POper, AVAIL y CONT. Algunas se ejecutan
 * en el mismo punto que una verificación de tipos, pero sus responsabilidades
 * son distintas.
 */
namespace GeneracionCodigo {

/** Coloca un operando textual en PilaO. */
void apilarOperando(
    EstadoSemantico& estado,
    const std::string& operando
);

/** Coloca un operador reconocido en POper. */
void guardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
);

/** Inserta o reutiliza la marca de apertura de un paréntesis. */
void abrirParentesis(EstadoSemantico& estado);

/** Reduce operadores hasta encontrar y quitar '('. */
void cerrarParentesis(EstadoSemantico& estado);

/** Obtiene un temporal nuevo o reutilizado desde AVAIL. */
std::string obtenerTemporalAvail(EstadoSemantico& estado);

/** Devuelve un temporal que ya no se necesita a AVAIL. */
void liberarTemporalAvail(
    EstadoSemantico& estado,
    const std::string& operando
);

/** Crea un cuádruplo y devuelve su índice lógico en CONT. */
int generarCuadruplo(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& op1,
    const std::string& op2,
    const std::string& resultado
);

/** Valida y genera una operación binaria con dos operandos. */
void reducirOperacion(
    EstadoSemantico& estado,
    const std::string& operador
);

/** Valida y genera una operación unaria, actualmente '!'. */
void reducirOperacionUnaria(
    EstadoSemantico& estado,
    const std::string& operador
);

/** Genera la asignación del resultado de una expresión al destino. */
void generarAsignacion(
    EstadoSemantico& estado,
    const std::string& expresion
);

/** Devuelve la prioridad del operador o -1 si no es reconocido. */
int precedencia(const std::string& operador);

} // namespace GeneracionCodigo

/**
 * Acciones de GENERACIÓN Y RELLENADO DE SALTOS.
 */
namespace ControlFlujo {

/** Genera un salto pendiente y lo registra en PSaltos. */
int generarSalto(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& operando
);

/** Completa el destino de un salto ya generado. */
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
