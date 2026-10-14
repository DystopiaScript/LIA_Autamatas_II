#include "SemanticParser.h"

#include <stdexcept>

namespace Semantica {

// ============================================================================
// DESCRIPCION GENERAL
//
// Este archivo contiene las operaciones que el parser invoca como acciones
// semanticas. No decide que produccion usar: esa responsabilidad pertenece a
// Parser y a su matriz predictiva.
//
// Aqui se implementan:
// - tabla de simbolos y tipos;
// - cubo semantico;
// - PilaO, PTipos y POper;
// - temporales AVAIL y contador CONT;
// - generacion de cuadruplos;
// - validacion de asignaciones y entrada/salida;
// - PSaltos y backpatching para control de flujo futuro.
// ============================================================================

namespace {

// ============================================================================
// UTILIDADES INTERNAS
// ============================================================================

bool esNumerico(TipoDato tipo) {
    return tipo == TipoDato::ENTERO || tipo == TipoDato::REAL;
}

bool esRelacional(const std::string& operador) {
    return operador == "==" || operador == "!=" ||
           operador == "<" || operador == "<=" ||
           operador == ">" || operador == ">=";
}

void exigirPilasAlineadas(const EstadoSemantico& estado) {
    if (estado.PilaO.size() != estado.PTipos.size()) {
        throw std::logic_error(
            "[EXPRESIONES] PilaO y PTipos deben contener la misma cantidad "
            "de elementos"
        );
    }
}

// Centraliza la diferencia entre una reducción binaria y la negación unaria.
// Así los paréntesis y el fin de expresión aplican exactamente la misma regla.
void reducirOperadorPendiente(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (operador == "!" || operador == "NEG") {
        GeneracionCodigo::reducirOperacionUnaria(estado, operador);
    } else {
        GeneracionCodigo::reducirOperacion(estado, operador);
    }
}

} // namespace

// ============================================================================
// REPRESENTACION DE TIPOS
// ============================================================================

std::string tipoToString(TipoDato tipo) {
    switch (tipo) {
        case TipoDato::ENTERO: return "int";
        case TipoDato::REAL: return "float";
        case TipoDato::CARACTER: return "char";
        case TipoDato::BOOLEANO: return "bool";
        case TipoDato::CADENA: return "string";
        case TipoDato::ERROR_SEMANTICO:
        default: return "ERROR_SEMANTICO";
    }
}

// ============================================================================
// MODULO 1: VERIFICACION SEMANTICA
//
// Responsabilidad:
// - tabla de simbolos;
// - tipos de operandos;
// - cubo semantico;
// - compatibilidad de operaciones y asignaciones.
//
// Este modulo NO genera cuadruplos.
// ============================================================================

namespace VerificacionSemantica {

// --------------------------------------------------------------------------
// MODULO 1: VERIFICACION SEMANTICA
//
// Estas funciones responden preguntas como:
// - ¿el identificador existe?
// - ¿ya fue declarado?
// - ¿los dos operandos son compatibles?
// - ¿el resultado puede asignarse al destino?
//
// La verificacion de tipos se hace antes de crear el cuadruplo, para no
// generar codigo intermedio basado en una operacion invalida.
// --------------------------------------------------------------------------

void registrarSimbolo(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo
) {
    if (id.empty() || tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "[TABLA DE SIMBOLOS] Identificador o tipo invalido"
        );
    }

    if (estado.tablaSimbolos.find(id) != estado.tablaSimbolos.end()) {
        throw std::runtime_error(
            "[TABLA DE SIMBOLOS] Identificador redeclarado: " + id
        );
    }

    Simbolo simbolo;
    simbolo.id = id;
    simbolo.tipo = tipo;
    // En esta etapa no reservamos memoria real del sistema operativo. El
    // compilador construye su propio mapa de memoria abstracta: 0, 1, 2...
    // Luego el generador de codigo o la maquina virtual puede transformar
    // esas posiciones en offsets, celdas o direcciones reales.
    simbolo.direccionRAM = estado.siguienteDireccionRAM++;
    estado.tablaSimbolos.emplace(id, simbolo);
}

const Simbolo& buscarSimbolo(
    const EstadoSemantico& estado,
    const std::string& id
) {
    const auto it = estado.tablaSimbolos.find(id);
    if (it == estado.tablaSimbolos.end()) {
        throw std::runtime_error(
            "[TABLA DE SIMBOLOS] Identificador no declarado: " + id
        );
    }
    return it->second;
}

void apilarTipo(EstadoSemantico& estado, TipoDato tipo) {
    if (tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "[TIPOS] No se puede apilar un tipo invalido"
        );
    }
    estado.PTipos.push(tipo);
}

TipoDato CuboSemantico(
    TipoDato t1,
    const std::string& operador,
    TipoDato t2
) {
    // El cubo recibe los dos tipos y el operador. Su resultado es el tipo
    // que tendra la expresion o ERROR_SEMANTICO si la combinacion no existe.
    if (t1 == TipoDato::ERROR_SEMANTICO ||
        t2 == TipoDato::ERROR_SEMANTICO) {
        return TipoDato::ERROR_SEMANTICO;
    }

    if (operador == "+" || operador == "-" ||
        operador == "*" || operador == "/") {
        if (!esNumerico(t1) || !esNumerico(t2)) {
            return TipoDato::ERROR_SEMANTICO;
        }
        return t1 == TipoDato::REAL || t2 == TipoDato::REAL
            ? TipoDato::REAL
            : TipoDato::ENTERO;
    }

    if (operador == "%") {
        return t1 == TipoDato::ENTERO && t2 == TipoDato::ENTERO
            ? TipoDato::ENTERO
            : TipoDato::ERROR_SEMANTICO;
    }

    if (operador == "and" || operador == "&&" ||
        operador == "or" || operador == "||") {
        return t1 == TipoDato::BOOLEANO && t2 == TipoDato::BOOLEANO
            ? TipoDato::BOOLEANO
            : TipoDato::ERROR_SEMANTICO;
    }

    if (esRelacional(operador)) {
        if (esNumerico(t1) && esNumerico(t2)) {
            return TipoDato::BOOLEANO;
        }
        if ((operador == "==" || operador == "!=") && t1 == t2) {
            return TipoDato::BOOLEANO;
        }
    }

    return TipoDato::ERROR_SEMANTICO;
}

void validarOperacion(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (estado.PTipos.size() < 2) {
        throw std::logic_error(
            "[TIPOS] Faltan tipos para validar la operacion binaria"
        );
    }

    auto tipos = estado.PTipos;
    const TipoDato t2 = tipos.top();
    tipos.pop();
    const TipoDato t1 = tipos.top();
    const TipoDato resultado = CuboSemantico(t1, operador, t2);

    if (resultado == TipoDato::ERROR_SEMANTICO) {
        throw std::runtime_error(
            "[TIPOS] Tipos incompatibles para el operador '" +
            operador + "'"
        );
    }

    estado.PTipos.pop();
    estado.PTipos.pop();
    estado.PTipos.push(resultado);
}

void validarOperacionUnaria(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (estado.PTipos.empty()) {
        throw std::logic_error(
            "[TIPOS] Falta tipo para validar la operacion unaria"
        );
    }

    const TipoDato tipo = estado.PTipos.top();
    const bool negacionLogica =
        operador == "!" && tipo == TipoDato::BOOLEANO;
    const bool negacionNumerica =
        operador == "NEG" && esNumerico(tipo);

    if (!negacionLogica && !negacionNumerica) {
        throw std::runtime_error(
            "[TIPOS] Tipo incompatible para el operador unario '" +
            operador + "'"
        );
    }
    estado.PTipos.pop();
    estado.PTipos.push(
        negacionLogica ? TipoDato::BOOLEANO : tipo
    );
}

bool tiposCompatiblesAsignacion(TipoDato destino, TipoDato fuente) {
    return destino == fuente ||
           (destino == TipoDato::REAL && fuente == TipoDato::ENTERO);
}

void iniciarAsignacion(
    EstadoSemantico& estado,
    const std::string& idDestino
) {
    const Simbolo& simbolo = buscarSimbolo(estado, idDestino);
    if (simbolo.esConstante) {
        throw std::runtime_error(
            "[ASIGNACION] Una constante no puede ser destino: " +
            idDestino
        );
    }
    estado.idDestinoAsignacion = idDestino;
    estado.tipoDestinoAsignacion = simbolo.tipo;
}

void validarAsignacion(
    EstadoSemantico& estado,
    TipoDato tipoExpresion
) {
    if (!tiposCompatiblesAsignacion(
            estado.tipoDestinoAsignacion,
            tipoExpresion)) {
        throw std::runtime_error(
            "[TIPOS] Incompatibilidad de tipos en la asignacion de '" +
            estado.idDestinoAsignacion + "'"
        );
    }
}

void acumularIdentificadorDeclaracion(
    EstadoSemantico& estado,
    const std::string& id
) {
    estado.identificadoresPendientes.push_back(id);
}

void registrarDeclaracion(
    EstadoSemantico& estado,
    TipoDato tipo
) {
    for (const std::string& id : estado.identificadoresPendientes) {
        registrarSimbolo(estado, id, tipo);
    }
    estado.identificadoresPendientes.clear();
}

} // namespace VerificacionSemantica

// ============================================================================
// MODULO 2: DECLARACIONES
//
// Las declaraciones de variables acumulan ids y les asignan un tipo al llegar
// a TIPO. Las constantes se registran con valor y no pueden ser destinos de
// asignacion o entrada.
// ============================================================================

namespace Declaraciones {

// --------------------------------------------------------------------------
// MODULO 2: DECLARACIONES
//
// La gramatica reconoce primero los nombres y despues el tipo:
//
//     def x, y : int ;
//
// Por eso los identificadores se almacenan temporalmente en
// identificadoresPendientes hasta que aparece int, float, char, bool o
// string.
// --------------------------------------------------------------------------

void iniciarDeclaracion(EstadoSemantico& estado) {
    estado.identificadoresPendientes.clear();
}

void acumularIdentificador(
    EstadoSemantico& estado,
    const std::string& id
) {
    if (id.empty()) {
        throw std::invalid_argument(
            "[DECLARACIONES] No se puede declarar un identificador vacio"
        );
    }
    VerificacionSemantica::acumularIdentificadorDeclaracion(estado, id);
}

void declararVariables(
    EstadoSemantico& estado,
    TipoDato tipo
) {
    VerificacionSemantica::registrarDeclaracion(estado, tipo);
}

void declararConstante(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo,
    const std::string& valor
) {
    if (id.empty() || valor.empty() ||
        tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "[DECLARACIONES] Datos invalidos para declarar una constante"
        );
    }

    if (estado.tablaSimbolos.find(id) != estado.tablaSimbolos.end()) {
        throw std::runtime_error(
            "[DECLARACIONES] Identificador redeclarado: " + id
        );
    }

    Simbolo simbolo;
    simbolo.id = id;
    simbolo.tipo = tipo;
    simbolo.direccionRAM = estado.siguienteDireccionRAM++;
    simbolo.esConstante = true;
    simbolo.inicializada = true;
    simbolo.valorConstante = valor;
    estado.tablaSimbolos.emplace(id, simbolo);
}

} // namespace Declaraciones

// ============================================================================
// MODULO 3: EXPRESIONES
//
// Este modulo representa las acciones del diagrama de expresiones:
// FACT, TERM, EXPR5, relaciones, &&, ||, ! y parentesis.
// PilaO y PTipos siempre avanzan sincronizadas. La reduccion delega la
// validacion al cubo semantico y la generacion al modulo de cuadruplos.
// ============================================================================

namespace Expresiones {

// --------------------------------------------------------------------------
// MODULO 3: EXPRESIONES
//
// Las acciones de este modulo implementan el esquema clasico:
//
//     operando -> PilaO y su tipo -> PTipos
//     operador -> POper
//     reduccion -> cubo semantico + temporal + cuadruplo
//
// PilaO y PTipos siempre deben permanecer alineadas: el elemento de una pila
// describe al elemento que ocupa la misma posicion en la otra.
// --------------------------------------------------------------------------

void apilarIdentificador(
    EstadoSemantico& estado,
    const std::string& id
) {
    // Una variable se representa por su direccion logica; una constante se
    // representa por su valor literal almacenado en la tabla de simbolos.
    const Simbolo& simbolo =
        VerificacionSemantica::buscarSimbolo(estado, id);
    if (simbolo.esConstante) {
        GeneracionCodigo::apilarOperando(
            estado,
            simbolo.valorConstante
        );
    } else {
        GeneracionCodigo::apilarOperando(
            estado,
            std::to_string(simbolo.direccionRAM)
        );
    }
    VerificacionSemantica::apilarTipo(estado, simbolo.tipo);
}

void apilarConstante(
    EstadoSemantico& estado,
    const std::string& valor,
    TipoDato tipo
) {
    // Las constantes no necesitan una entrada temporal en la tabla para ser
    // usadas en una expresion: se apilan directamente con su tipo.
    if (valor.empty() || tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "[EXPRESIONES] Constante invalida en expresion"
        );
    }
    GeneracionCodigo::apilarOperando(estado, valor);
    VerificacionSemantica::apilarTipo(estado, tipo);
}

void recibirOperador(
    EstadoSemantico& estado,
    const std::string& operador
) {
    GeneracionCodigo::guardarOperador(estado, operador);
}

void recibirOperadorUnario(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (operador != "!" && operador != "-") {
        throw std::invalid_argument(
            "[EXPRESIONES] Operador unario no reconocido: " + operador
        );
    }
    GeneracionCodigo::guardarOperador(
        estado,
        operador == "-" ? "NEG" : operador
    );
}

void abrirParentesis(EstadoSemantico& estado) {
    GeneracionCodigo::abrirParentesis(estado);
}

void cerrarParentesis(EstadoSemantico& estado) {
    while (!estado.POper.empty() && estado.POper.top() != "(") {
        const std::string operador = estado.POper.top();
        estado.POper.pop();
        reducirOperadorPendiente(estado, operador);
    }
    if (estado.POper.empty()) {
        throw std::logic_error(
            "[EXPRESIONES] Parentesis de cierre sin marca de apertura"
        );
    }
    estado.POper.pop();
}

void finalizarOperacionPendiente(EstadoSemantico& estado) {
    while (!estado.POper.empty() && estado.POper.top() != "(") {
        const std::string operador = estado.POper.top();
        estado.POper.pop();
        reducirOperadorPendiente(estado, operador);
    }
}

std::string obtenerResultado(EstadoSemantico& estado) {
    if (estado.PilaO.empty() || estado.PTipos.empty()) {
        throw std::logic_error(
            "[EXPRESIONES] La expresion no produjo un resultado"
        );
    }
    if (estado.PilaO.size() != estado.PTipos.size()) {
        throw std::logic_error(
            "[EXPRESIONES] PilaO y PTipos deben contener la misma cantidad "
            "de elementos"
        );
    }

    const std::string resultado = estado.PilaO.top();
    estado.PilaO.pop();
    estado.PTipos.pop();
    return resultado;
}

void finalizarAsignacion(EstadoSemantico& estado) {
    // La asignacion se cierra cuando el parser reconoce ';'. Para este
    // momento la expresion ya fue reducida y queda un unico resultado en las
    // pilas de trabajo.
    // La asignación se cierra fuera de POper: '=' pertenece a EST_ASIG,
    // mientras que POper únicamente contiene operadores de EXPR.
    if (estado.idDestinoAsignacion.empty() ||
        estado.tipoDestinoAsignacion == TipoDato::ERROR_SEMANTICO) {
        throw std::logic_error(
            "[ASIGNACION] No existe un destino de asignacion activo"
        );
    }
    if (estado.PilaO.empty() || estado.PTipos.empty()) {
        throw std::logic_error(
            "[ASIGNACION] La expresion no produjo un resultado"
        );
    }
    if (estado.PilaO.size() != estado.PTipos.size()) {
        throw std::logic_error(
            "[ASIGNACION] PilaO y PTipos estan desalineadas"
        );
    }

    VerificacionSemantica::validarAsignacion(
        estado,
        estado.PTipos.top()
    );

    const std::string expresion = estado.PilaO.top();
    estado.PilaO.pop();
    estado.PTipos.pop();
    GeneracionCodigo::generarAsignacion(estado, expresion);

    estado.idDestinoAsignacion.clear();
    estado.tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
}

} // namespace Expresiones

// ============================================================================
// MODULO 4: ENTRADA Y SALIDA
//
// input solo acepta variables declaradas. output recibe el resultado de una
// expresion ya evaluada. Ambas acciones generan un cuadruplo.
// ============================================================================

namespace EntradaSalida {

// --------------------------------------------------------------------------
// MODULO 4: ENTRADA Y SALIDA
//
// Estas funciones validan destinos y generan cuadruplos input/output. La
// forma exacta aceptada la determina la gramatica actual del Parser.
// --------------------------------------------------------------------------

void validarDestinoEntrada(
    const EstadoSemantico& estado,
    const std::string& id
) {
    const Simbolo& simbolo =
        VerificacionSemantica::buscarSimbolo(estado, id);
    if (simbolo.esConstante) {
        throw std::runtime_error(
            "[ENTRADA] Una constante no puede ser destino de input: " + id
        );
    }
}

int generarEntrada(
    EstadoSemantico& estado,
    const std::string& id
) {
    validarDestinoEntrada(estado, id);
    const int indice = GeneracionCodigo::generarCuadruplo(
        estado,
        "input",
        "_",
        "_",
        id
    );
    estado.tablaSimbolos.at(id).inicializada = true;
    return indice;
}

int generarSalida(
    EstadoSemantico& estado,
    const std::string& resultadoExpresion
) {
    if (resultadoExpresion.empty()) {
        throw std::invalid_argument(
            "[SALIDA] No se puede generar output sin expresion"
        );
    }
    const int indice = GeneracionCodigo::generarCuadruplo(
        estado,
        "output",
        resultadoExpresion,
        "_",
        "_"
    );
    GeneracionCodigo::liberarTemporalAvail(
        estado,
        resultadoExpresion
    );
    return indice;
}

} // namespace EntradaSalida

// ============================================================================
// MODULO 5: GENERACION DE CODIGO INTERMEDIO
//
// Responsabilidad:
// - PilaO y POper;
// - AVAIL para temporales;
// - CONT y vector de cuadruplos;
// - reduccion de operaciones y asignaciones.
//
// La validacion de tipos se realiza llamando a VerificacionSemantica.
// ============================================================================

namespace GeneracionCodigo {

// --------------------------------------------------------------------------
// MODULO 5: GENERACION DE CODIGO INTERMEDIO
//
// Aqui se transforma una reduccion semantica en un cuadruplo:
//
//     op1 operador op2 -> temporal
//
// AVAIL permite reutilizar temporales que ya dejaron de ser necesarios.
// CONT identifica el numero consecutivo de cada cuadruplo.
// --------------------------------------------------------------------------

void apilarOperando(
    EstadoSemantico& estado,
    const std::string& operando
) {
    estado.PilaO.push(operando);
}

void guardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (precedencia(operador) < 0) {
        throw std::invalid_argument(
            "[CUADRUPLOS] Operador no reconocido: " + operador
        );
    }
    estado.POper.push(operador);
}

void abrirParentesis(EstadoSemantico& estado) {
    estado.POper.push("(");
}

void cerrarParentesis(EstadoSemantico& estado) {
    while (!estado.POper.empty() && estado.POper.top() != "(") {
        const std::string operador = estado.POper.top();
        estado.POper.pop();
        reducirOperadorPendiente(estado, operador);
    }

    if (estado.POper.empty()) {
        throw std::logic_error(
            "[CUADRUPLOS] Parentesis de cierre sin marca de apertura"
        );
    }
    estado.POper.pop();
}

std::string obtenerTemporalAvail(EstadoSemantico& estado) {
    if (!estado.temporalesDisponibles.empty()) {
        const std::string temporal = estado.temporalesDisponibles.top();
        estado.temporalesDisponibles.pop();
        return temporal;
    }
    return "$T" + std::to_string(estado.siguienteTemporal++);
}

void liberarTemporalAvail(
    EstadoSemantico& estado,
    const std::string& operando
) {
    if (operando.size() > 2 &&
        operando[0] == '$' &&
        operando[1] == 'T') {
        estado.temporalesDisponibles.push(operando);
    }
}

int generarCuadruplo(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& op1,
    const std::string& op2,
    const std::string& resultado
) {
    const int indice = estado.CONT++;
    estado.cuadruplos.push_back({operador, op1, op2, resultado});
    return indice;
}

void reducirOperacion(
    EstadoSemantico& estado,
    const std::string& operador
) {
    // La validacion ocurre antes de extraer los operandos. Si el cubo falla,
    // las pilas conservan su informacion para facilitar el diagnostico.
    exigirPilasAlineadas(estado);
    if (estado.PilaO.size() < 2) {
        throw std::logic_error(
            "[CUADRUPLOS] Faltan operandos para reducir operacion binaria"
        );
    }

    VerificacionSemantica::validarOperacion(estado, operador);

    const std::string op2 = estado.PilaO.top();
    estado.PilaO.pop();
    const std::string op1 = estado.PilaO.top();
    estado.PilaO.pop();

    const std::string temporal = obtenerTemporalAvail(estado);
    generarCuadruplo(estado, operador, op1, op2, temporal);
    liberarTemporalAvail(estado, op1);
    liberarTemporalAvail(estado, op2);
    apilarOperando(estado, temporal);
}

void reducirOperacionUnaria(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (estado.PilaO.empty() || estado.PTipos.empty()) {
        throw std::logic_error(
            "[CUADRUPLOS] Faltan operandos para reducir la operacion unaria"
        );
    }

    VerificacionSemantica::validarOperacionUnaria(estado, operador);

    const std::string operando = estado.PilaO.top();
    estado.PilaO.pop();

    const std::string temporal = obtenerTemporalAvail(estado);
    generarCuadruplo(estado, operador, operando, "_", temporal);
    liberarTemporalAvail(estado, operando);
    apilarOperando(estado, temporal);
}

void generarAsignacion(
    EstadoSemantico& estado,
    const std::string& expresion
) {
    generarCuadruplo(
        estado,
        ":=",
        expresion,
        "_",
        estado.idDestinoAsignacion
    );
    liberarTemporalAvail(estado, expresion);
    estado.tablaSimbolos.at(
        estado.idDestinoAsignacion
    ).inicializada = true;
}

int precedencia(const std::string& operador) {
    if (operador == "or" || operador == "||") return 1;
    if (operador == "and" || operador == "&&") return 2;
    if (esRelacional(operador)) return 3;
    if (operador == "+" || operador == "-") return 4;
    if (operador == "*" || operador == "/" || operador == "%") return 5;
    if (operador == "!" || operador == "NEG") return 6;
    return -1;
}

} // namespace GeneracionCodigo

// ============================================================================
// MODULO 6: CONTROL DE FLUJO
//
// PSaltos conserva los indices de GOTOF/GOTO pendientes y completarSalto
// realiza el backpatching cuando ya se conoce el destino.
// ============================================================================

namespace ControlFlujo {

// --------------------------------------------------------------------------
// MODULO 6: CONTROL DE FLUJO
//
// PSaltos conserva cuadruplos cuyo destino aun se desconoce. La funcion de
// backpatching completa posteriormente el campo RES con la instruccion real.
// --------------------------------------------------------------------------

int generarSalto(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& operando
) {
    const int indice = GeneracionCodigo::generarCuadruplo(
        estado,
        operador,
        operando,
        "_",
        "___"
    );
    estado.PSaltos.push(indice);
    return indice;
}

void completarSalto(
    EstadoSemantico& estado,
    int indiceCuadruplo,
    int destino
) {
    if (indiceCuadruplo < 0 ||
        indiceCuadruplo >= static_cast<int>(estado.cuadruplos.size())) {
        throw std::out_of_range(
            "[SALTOS] Indice de cuadruplo invalido durante backpatching"
        );
    }
    estado.cuadruplos[indiceCuadruplo].res = std::to_string(destino);
}

} // namespace ControlFlujo

// ============================================================================
// LIMPIEZA DEL ESTADO
// ============================================================================

void reiniciarEstado(EstadoSemantico& estado) {
    // Cada pulsacion de Analizar debe iniciar desde un estado limpio. Si no
    // se vaciaran las pilas, los simbolos y CONT de una ejecucion anterior
    // contaminarian la siguiente.
    estado.tablaSimbolos.clear();
    estado.cuadruplos.clear();
    estado.identificadoresPendientes.clear();
    estado.idDestinoAsignacion.clear();
    estado.tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
    estado.siguienteDireccionRAM = 0;
    estado.siguienteTemporal = 1;
    estado.CONT = 1;

    while (!estado.PilaO.empty()) estado.PilaO.pop();
    while (!estado.PTipos.empty()) estado.PTipos.pop();
    while (!estado.POper.empty()) estado.POper.pop();
    while (!estado.PSaltos.empty()) estado.PSaltos.pop();
    while (!estado.temporalesDisponibles.empty()) {
        estado.temporalesDisponibles.pop();
    }
}

} // namespace Semantica
