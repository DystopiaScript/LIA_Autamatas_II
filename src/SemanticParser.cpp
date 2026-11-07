#include "SemanticParser.h"

#include <cmath>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

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

std::string claveVariableRAM(int direccionRAM) {
    return "@VRAM[" + std::to_string(direccionRAM) + "]";
}

std::string operandoVisible(const std::string& operando) {
    const std::string prefijo = "@VRAM[";
    if (operando.size() > prefijo.size() &&
        operando.compare(0, prefijo.size(), prefijo) == 0 &&
        operando.back() == ']') {
        return operando.substr(
            prefijo.size(),
            operando.size() - prefijo.size() - 1
        );
    }
    return operando;
}

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

std::string formatearNumero(double valor, bool entero) {
    if (entero) {
        return std::to_string(static_cast<long long>(valor));
    }
    std::ostringstream salida;
    salida << std::setprecision(12) << valor;
    return salida.str();
}

std::string calcularValor(
    const std::string& operador,
    const std::string& op1,
    const std::string& op2
) {
    const bool operandosBooleanos =
        (op1 == "true" || op1 == "false") &&
        (op2 == "true" || op2 == "false");
    if (operandosBooleanos) {
        const bool valor1 = op1 == "true";
        const bool valor2 = op2 == "true";

        if (operador == "&&" || operador == "and") {
            return (valor1 && valor2) ? "true" : "false";
        }
        if (operador == "||" || operador == "or") {
            return (valor1 || valor2) ? "true" : "false";
        }
        if (operador == "==" || operador == "!=") {
            const bool iguales = valor1 == valor2;
            return (operador == "==" ? iguales : !iguales)
                ? "true"
                : "false";
        }
    }

    const bool esEntero =
        op1.find('.') == std::string::npos &&
        op2.find('.') == std::string::npos;
    double valor1 = 0;
    double valor2 = 0;
    try {
        valor1 = std::stod(op1);
        valor2 = std::stod(op2);
    } catch (...) {
        return {};
    }

    if (operador == "+") return formatearNumero(valor1 + valor2, esEntero);
    if (operador == "-") return formatearNumero(valor1 - valor2, esEntero);
    if (operador == "*") return formatearNumero(valor1 * valor2, esEntero);
    if (operador == "/") {
        if (valor2 == 0) return {};
        return formatearNumero(valor1 / valor2, false);
    }
    if (operador == "%") {
        if (static_cast<long long>(valor2) == 0) return {};
        return std::to_string(
            static_cast<long long>(valor1) %
            static_cast<long long>(valor2)
        );
    }
    if (operador == "==" || operador == "!=" ||
        operador == "<" || operador == "<=" ||
        operador == ">" || operador == ">=") {
        bool resultado = false;
        if (operador == "==") resultado = valor1 == valor2;
        if (operador == "!=") resultado = valor1 != valor2;
        if (operador == "<") resultado = valor1 < valor2;
        if (operador == "<=") resultado = valor1 <= valor2;
        if (operador == ">") resultado = valor1 > valor2;
        if (operador == ">=") resultado = valor1 >= valor2;
        return resultado ? "true" : "false";
    }
    return {};
}

} // namespace

int operadorACodigo(const std::string& op) {
    if (op == "+") return OP_NUM_SUMA;
    if (op == "-") return OP_NUM_RESTA;
    if (op == "*") return OP_NUM_MULT;
    if (op == "/") return OP_NUM_DIV;
    if (op == "%") return OP_NUM_MODULO;
    if (op == "=") return OP_NUM_ASIG;
    if (op == "==") return OP_NUM_IGUAL;
    if (op == "<") return OP_NUM_MENOR;
    if (op == "<=") return OP_NUM_MENORIGUAL;
    if (op == ">") return OP_NUM_MAYOR;
    if (op == ">=") return OP_NUM_MAYORIGUAL;
    if (op == "!=") return OP_NUM_DIFERENTE;
    if (op == "!") return OP_NUM_NOT;
    if (op == "&&" || op == "and") return OP_NUM_AND;
    if (op == "||" || op == "or") return OP_NUM_OR;
    if (op == "(") return OP_NUM_PAREN_ABRE;
    if (op == "NEG") return OP_NUM_NEG;
    if (op == "gotofalso") return OP_NUM_GOTOFALSO;
    if (op == "goto") return OP_NUM_GOTO;
    if (op == "input") return OP_NUM_INPUT;
    if (op == "output") return OP_NUM_OUTPUT;
    return -1;
}

std::string codigoAOperador(int cod) {
    switch (cod) {
        case OP_NUM_SUMA: return "+";
        case OP_NUM_RESTA: return "-";
        case OP_NUM_MULT: return "*";
        case OP_NUM_DIV: return "/";
        case OP_NUM_MODULO: return "%";
        case OP_NUM_ASIG: return "=";
        case OP_NUM_IGUAL: return "==";
        case OP_NUM_MENOR: return "<";
        case OP_NUM_MENORIGUAL: return "<=";
        case OP_NUM_MAYOR: return ">";
        case OP_NUM_MAYORIGUAL: return ">=";
        case OP_NUM_DIFERENTE: return "!=";
        case OP_NUM_NOT: return "!";
        case OP_NUM_AND: return "&&";
        case OP_NUM_OR: return "||";
        case OP_NUM_PAREN_ABRE: return "(";
        case OP_NUM_NEG: return "NEG";
        case OP_NUM_GOTOFALSO: return "gotofalso";
        case OP_NUM_GOTO: return "goto";
        case OP_NUM_INPUT: return "input";
        case OP_NUM_OUTPUT: return "output";
        default: return std::to_string(cod);
    }
}

int tipoACodigo(TipoDato tipo) {
    switch (tipo) {
        case TipoDato::ENTERO: return TIPO_NUM_ENTERO;
        case TipoDato::REAL: return TIPO_NUM_REAL;
        case TipoDato::CARACTER: return TIPO_NUM_CARACTER;
        case TipoDato::BOOLEANO: return TIPO_NUM_BOOLEANO;
        case TipoDato::CADENA: return TIPO_NUM_CADENA;
        case TipoDato::ERROR_SEMANTICO:
        default: return TIPO_NUM_ERROR;
    }
}

TipoDato codigoATipo(int cod) {
    switch (cod) {
        case TIPO_NUM_ENTERO: return TipoDato::ENTERO;
        case TIPO_NUM_REAL: return TipoDato::REAL;
        case TIPO_NUM_CARACTER: return TipoDato::CARACTER;
        case TIPO_NUM_BOOLEANO: return TipoDato::BOOLEANO;
        case TIPO_NUM_CADENA: return TipoDato::CADENA;
        default: return TipoDato::ERROR_SEMANTICO;
    }
}

int registrarLiteral(EstadoSemantico& estado, const std::string& valor, TipoDato tipo) {
    (void)tipo;
    auto it = estado.literalAId.find(valor);
    if (it != estado.literalAId.end()) {
        return it->second;
    }
    int id = estado.siguienteLiteral++;
    estado.tablaLiterales[id] = valor;
    estado.literalAId[valor] = id;
    return id;
}

int operandoANumero(const std::string& op, EstadoSemantico& estado) {
    if (op.empty() || op == "_" || op == "___") {
        return OPERANDO_NULO;
    }
    const std::string prefijoRAM = "@VRAM[";
    if (op.size() > prefijoRAM.size() &&
        op.compare(0, prefijoRAM.size(), prefijoRAM) == 0 &&
        op.back() == ']') {
        return std::stoi(op.substr(prefijoRAM.size(), op.size() - prefijoRAM.size() - 1));
    }
    if (op.size() >= 2 && op[0] == '$' && op[1] == 'T') {
        return BASE_TEMPORALES + std::stoi(op.substr(2));
    }
    auto itSim = estado.tablaSimbolos.find(op);
    if (itSim != estado.tablaSimbolos.end()) {
        return itSim->second.direccionRAM;
    }

    // Es un valor literal numérico, booleano o cadena
    return registrarLiteral(estado, op, TipoDato::ERROR_SEMANTICO);
}

std::string numeroAOperandoVisual(int cod, const EstadoSemantico& estado) {
    if (cod == OPERANDO_NULO) {
        return "_";
    }
    if (cod >= 0 && cod < BASE_TEMPORALES) {
        return std::to_string(cod);
    }
    if (cod > BASE_TEMPORALES && cod < BASE_CONSTANTES) {
        return "$T" + std::to_string(cod - BASE_TEMPORALES);
    }
    if (cod > BASE_CONSTANTES) {
        auto it = estado.tablaLiterales.find(cod);
        if (it != estado.tablaLiterales.end()) {
            return it->second;
        }
    }
    return std::to_string(cod);
}

std::string numeroAOperandoRaw(int cod, const EstadoSemantico& estado) {
    if (cod == OPERANDO_NULO) {
        return "_";
    }
    if (cod >= 0 && cod < BASE_TEMPORALES) {
        return claveVariableRAM(cod);
    }
    if (cod > BASE_TEMPORALES && cod < BASE_CONSTANTES) {
        return "$T" + std::to_string(cod - BASE_TEMPORALES);
    }
    if (cod > BASE_CONSTANTES) {
        auto it = estado.tablaLiterales.find(cod);
        if (it != estado.tablaLiterales.end()) {
            return it->second;
        }
    }
    return std::to_string(cod);
}

std::string resolverOperandoVisual(const EstadoSemantico& estado, int opNum) {
    return numeroAOperandoVisual(opNum, estado);
}

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
    estado.PTipos.push(tipoACodigo(tipo));
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
    const TipoDato t2 = codigoATipo(tipos.top());
    tipos.pop();
    const TipoDato t1 = codigoATipo(tipos.top());
    const TipoDato resultado = CuboSemantico(t1, operador, t2);

    if (resultado == TipoDato::ERROR_SEMANTICO) {
        throw std::runtime_error(
            "[TIPOS] Tipos incompatibles para el operador '" +
            operador + "'"
        );
    }

    estado.PTipos.pop();
    estado.PTipos.pop();
    estado.PTipos.push(tipoACodigo(resultado));
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

    const TipoDato tipo = codigoATipo(estado.PTipos.top());
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
        tipoACodigo(negacionLogica ? TipoDato::BOOLEANO : tipo)
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
    // La busqueda ocurre antes de analizar la expresion derecha. Asi se
    // rechaza inmediatamente `PI = ...` y nunca se genera codigo que intente
    // modificar una constante.
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
    // Esta es la segunda validacion de la asignacion: primero se valida el
    // destino, y aqui se compara el tipo de la expresion ya reducida con el
    // tipo declarado. ENTERO -> REAL es la unica conversion implicita.
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
    // Esta funcion recibe la informacion que Parser fue acumulando durante:
    //
    //     const id : TIPO = FACT ;
    //
    // Registrar aqui, y no al leer `const`, evita crear simbolos incompletos.
    if (id.empty() || valor.empty() ||
        tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "[DECLARACIONES] Datos invalidos para declarar una constante"
        );
    }

    // En esta primera sintaxis la constante debe inicializarse con un literal
    // compatible con el tipo declarado. Las expresiones como `const A : int =
    // 2 + 3` se pueden habilitar después con una producción más amplia.
    const bool esReal =
        valor.find('.') != std::string::npos ||
        valor.find('e') != std::string::npos ||
        valor.find('E') != std::string::npos;
    if (tipo == TipoDato::ENTERO && esReal) {
        throw std::runtime_error(
            "[CONSTANTES] Un valor real no puede declararse como ENTERO"
        );
    }
    if (tipo == TipoDato::REAL && valor.find('\'') != std::string::npos) {
        throw std::runtime_error(
            "[CONSTANTES] El valor de REAL debe ser numerico"
        );
    }
    if (tipo == TipoDato::CARACTER &&
        !(valor.size() >= 3 && valor.front() == '\'' && valor.back() == '\'')) {
        throw std::runtime_error(
            "[CONSTANTES] El valor de CARACTER debe usar comillas simples"
        );
    }
    if (tipo == TipoDato::CADENA &&
        !(valor.size() >= 2 && valor.front() == '"' && valor.back() == '"')) {
        throw std::runtime_error(
            "[CONSTANTES] El valor de CADENA debe usar comillas dobles"
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
    simbolo.valorConocido = true;
    simbolo.valorActual = valor;
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
    // Una variable se representa en una expresion por su direccion logica;
    // una constante se representa por su valor literal almacenado. Ambos
    // caminos apilan despues el tipo, manteniendo PilaO y PTipos alineadas.
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
            claveVariableRAM(simbolo.direccionRAM)
        );
    }
    if (simbolo.valorConocido) {
        estado.valoresTemporales[
            simbolo.esConstante
                ? simbolo.valorConstante
                : claveVariableRAM(simbolo.direccionRAM)
        ] = simbolo.valorActual;
    }
    VerificacionSemantica::apilarTipo(estado, simbolo.tipo);
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "PUSH tipo: " + tipoToString(simbolo.tipo)
    );
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
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "PUSH constante: " + valor
    );
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
    while (!estado.POper.empty() && estado.POper.top() != OP_NUM_PAREN_ABRE) {
        const int codOp = estado.POper.top();
        estado.POper.pop();
        reducirOperadorPendiente(estado, codigoAOperador(codOp));
    }
    if (estado.POper.empty()) {
        throw std::logic_error(
            "[EXPRESIONES] Parentesis de cierre sin marca de apertura"
        );
    }
    estado.POper.pop();
}

void finalizarOperacionPendiente(EstadoSemantico& estado) {
    while (!estado.POper.empty() && estado.POper.top() != OP_NUM_PAREN_ABRE) {
        const int codOp = estado.POper.top();
        estado.POper.pop();
        reducirOperadorPendiente(estado, codigoAOperador(codOp));
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

    const int resCod = estado.PilaO.top();
    estado.PilaO.pop();
    estado.PTipos.pop();
    return numeroAOperandoRaw(resCod, estado);
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
        codigoATipo(estado.PTipos.top())
    );

    const int resCod = estado.PilaO.top();
    estado.PilaO.pop();
    estado.PTipos.pop();
    GeneracionCodigo::generarAsignacion(estado, numeroAOperandoRaw(resCod, estado));

    estado.idDestinoAsignacion.clear();
    estado.tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
}

} // namespace Expresiones

// ============================================================================
// MODULO 4: ENTRADA Y SALIDA
//
// input solo acepta variables declaradas. output recibe el resultado de una
// expresion ya evaluada y lo coloca en RESULTADO del cuadruplo:
//
//     (output, _, _, resultado)
//
// Ambas acciones generan un cuadruplo.
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
    // input invalida el valor conocido porque la ejecucion real aun no ha
    // ocurrido. La variable sigue marcada como inicializada, pero su valor
    // estatico se muestra como desconocido.
    validarDestinoEntrada(estado, id);
    const int indice = GeneracionCodigo::generarCuadruplo(
        estado,
        "input",
        "_",
        "_",
        id
    );
    estado.tablaSimbolos.at(id).inicializada = true;
    estado.tablaSimbolos.at(id).valorConocido = false;
    estado.tablaSimbolos.at(id).valorActual.clear();
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "INPUT -> " + id
    );
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
        "_",
        "_",
        resultadoExpresion
    );
    GeneracionCodigo::liberarTemporalAvail(
        estado,
        resultadoExpresion
    );
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "OUTPUT <- " + resultadoExpresion
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

namespace {

std::string valorConocido(
    const EstadoSemantico& estado,
    const std::string& operando
) {
    // Busca primero resultados de temporales o variables propagados. Si no
    // existe una entrada, intenta reconocer un literal numerico o booleano.
    // Un identificador no inicializado y un resultado de input no entran.
    const auto temporal = estado.valoresTemporales.find(operando);
    if (temporal != estado.valoresTemporales.end()) {
        return temporal->second;
    }

    if (operando == "true" || operando == "false") {
        return operando;
    }

    try {
        std::stod(operando);
        return operando;
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace

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

void registrarEstadoPilas(
    EstadoSemantico& estado,
    const std::string& accion
) {
    HistorialPilas historial;
    historial.paso = static_cast<int>(estado.historialPilas.size()) + 1;
    historial.accion = accion;

    auto operandos = estado.PilaO;
    while (!operandos.empty()) {
        historial.operandosNum.push_back(operandos.top());
        historial.operandos.push_back(std::to_string(operandos.top()));
        operandos.pop();
    }
    std::reverse(historial.operandosNum.begin(), historial.operandosNum.end());
    std::reverse(historial.operandos.begin(), historial.operandos.end());

    auto tipos = estado.PTipos;
    while (!tipos.empty()) {
        historial.tiposNum.push_back(tipos.top());
        historial.tipos.push_back(std::to_string(tipos.top()));
        tipos.pop();
    }
    std::reverse(historial.tiposNum.begin(), historial.tiposNum.end());
    std::reverse(historial.tipos.begin(), historial.tipos.end());

    auto operadores = estado.POper;
    while (!operadores.empty()) {
        historial.operadoresNum.push_back(operadores.top());
        historial.operadores.push_back(std::to_string(operadores.top()));
        operadores.pop();
    }
    std::reverse(historial.operadoresNum.begin(), historial.operadoresNum.end());
    std::reverse(historial.operadores.begin(), historial.operadores.end());

    auto avail = estado.temporalesDisponibles;
    while (!avail.empty()) {
        historial.availNum.push_back(avail.top());
        historial.avail.push_back(std::to_string(avail.top()));
        avail.pop();
    }
    std::reverse(historial.availNum.begin(), historial.availNum.end());
    std::reverse(historial.avail.begin(), historial.avail.end());

    auto saltos = estado.PSaltos;
    while (!saltos.empty()) {
        historial.saltos.push_back(saltos.top());
        saltos.pop();
    }
    std::reverse(historial.saltos.begin(), historial.saltos.end());

    estado.historialPilas.push_back(std::move(historial));
}

void apilarOperando(
    EstadoSemantico& estado,
    const std::string& operando
) {
    const int opNum = operandoANumero(operando, estado);
    estado.PilaO.push(opNum);
    registrarEstadoPilas(estado, "PUSH operando: " + std::to_string(opNum));
}

void guardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
) {
    const int codOp = operadorACodigo(operador);
    if (codOp < 0) {
        throw std::invalid_argument(
            "[CUADRUPLOS] Operador no reconocido: " + operador
        );
    }
    estado.POper.push(codOp);
    registrarEstadoPilas(estado, "PUSH operador: " + std::to_string(codOp));
}

void abrirParentesis(EstadoSemantico& estado) {
    estado.POper.push(OP_NUM_PAREN_ABRE);
    registrarEstadoPilas(estado, "PUSH marca: " + std::to_string(OP_NUM_PAREN_ABRE));
}

void cerrarParentesis(EstadoSemantico& estado) {
    while (!estado.POper.empty() && estado.POper.top() != OP_NUM_PAREN_ABRE) {
        const int codOp = estado.POper.top();
        estado.POper.pop();
        reducirOperadorPendiente(estado, codigoAOperador(codOp));
    }

    if (estado.POper.empty()) {
        throw std::logic_error(
            "[CUADRUPLOS] Parentesis de cierre sin marca de apertura"
        );
    }
    estado.POper.pop();
    registrarEstadoPilas(estado, "POP marca: " + std::to_string(OP_NUM_PAREN_ABRE));
}

std::string obtenerTemporalAvail(EstadoSemantico& estado) {
    if (!estado.temporalesDisponibles.empty()) {
        const int tempCod = estado.temporalesDisponibles.top();
        estado.temporalesDisponibles.pop();
        return "$T" + std::to_string(tempCod - BASE_TEMPORALES);
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
        const int n = std::stoi(operando.substr(2));
        estado.temporalesDisponibles.push(BASE_TEMPORALES + n);
    }
}

int generarCuadruplo(
    EstadoSemantico& estado,
    const std::string& operador,
    const std::string& op1,
    const std::string& op2,
    const std::string& resultado
) {
    const int coperNum = operadorACodigo(operador);
    const int op1Num = operandoANumero(op1, estado);
    const int op2Num = operandoANumero(op2, estado);
    const int resNum = operandoANumero(resultado, estado);

    const int indice = estado.CONT++;
    estado.cuadruplos.push_back({
        coperNum,
        op1Num,
        op2Num,
        resNum,
        operador,
        operandoVisible(op1),
        operandoVisible(op2),
        operandoVisible(resultado),
        op1,
        op2,
        resultado
    });
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

    const int op2Cod = estado.PilaO.top();
    estado.PilaO.pop();
    const int op1Cod = estado.PilaO.top();
    estado.PilaO.pop();

    const std::string op2 = numeroAOperandoRaw(op2Cod, estado);
    const std::string op1 = numeroAOperandoRaw(op1Cod, estado);

    const std::string temporal = obtenerTemporalAvail(estado);
    generarCuadruplo(estado, operador, op1, op2, temporal);
    // Primero se genera el cuadruplo. Luego, solo si ambos operandos tienen
    // valor conocido, se calcula tambien el temporal para la interfaz.
    const std::string valor1 = valorConocido(estado, op1);
    const std::string valor2 = valorConocido(estado, op2);
    if (!valor1.empty() && !valor2.empty()) {
        try {
            estado.valoresTemporales[temporal] =
                calcularValor(operador, valor1, valor2);
        } catch (const std::exception&) {
            estado.valoresTemporales.erase(temporal);
        }
    }
    liberarTemporalAvail(estado, op1);
    liberarTemporalAvail(estado, op2);
    apilarOperando(estado, temporal);
    registrarEstadoPilas(
        estado,
        "REDUCCION " + op1 + " " + operador + " " + op2 +
        " -> " + temporal
    );
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

    const int opCod = estado.PilaO.top();
    estado.PilaO.pop();

    const std::string operando = numeroAOperandoRaw(opCod, estado);

    const std::string temporal = obtenerTemporalAvail(estado);
    generarCuadruplo(estado, operador, operando, "_", temporal);
    const std::string valor = valorConocido(estado, operando);
    if (!valor.empty()) {
        if (operador == "!" && (valor == "true" || valor == "false")) {
            estado.valoresTemporales[temporal] =
                valor == "true" ? "false" : "true";
        } else if (operador == "NEG") {
            try {
                estado.valoresTemporales[temporal] =
                    formatearNumero(-std::stod(valor), valor.find('.') == std::string::npos);
            } catch (const std::exception&) {
                estado.valoresTemporales.erase(temporal);
            }
        }
    }
    liberarTemporalAvail(estado, operando);
    apilarOperando(estado, temporal);
    registrarEstadoPilas(
        estado,
        "REDUCCION " + operador + " " + operando + " -> " + temporal
    );
}

void generarAsignacion(
    EstadoSemantico& estado,
    const std::string& expresion
) {
    // La asignacion siempre genera codigo intermedio. La propagacion de
    // valor es independiente: si la expresion es conocida, se copia al
    // simbolo destino; si no, se conserva como desconocida.
    generarCuadruplo(
        estado,
        "=",
        expresion,
        "_",
        estado.idDestinoAsignacion
    );
    liberarTemporalAvail(estado, expresion);
    Simbolo& destino = estado.tablaSimbolos.at(estado.idDestinoAsignacion);
    destino.inicializada = true;
    const std::string valor = valorConocido(estado, expresion);
    destino.valorConocido = !valor.empty();
    destino.valorActual = valor;
    if (destino.valorConocido) {
        estado.valoresTemporales[
            claveVariableRAM(destino.direccionRAM)
        ] = valor;
    }
    registrarEstadoPilas(
        estado,
        "ASIGNACION -> " + estado.idDestinoAsignacion
    );
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

void iniciarIf(EstadoSemantico& estado) {
    if (estado.PilaO.empty() || estado.PTipos.empty()) {
        throw std::logic_error(
            "[SALTOS] Falta la condicion del IF"
        );
    }

    const TipoDato tipoCondicion = codigoATipo(estado.PTipos.top());
    if (tipoCondicion != TipoDato::BOOLEANO) {
        throw std::logic_error(
            "[SEMANTICA] La condicion del IF debe ser de tipo BOOLEANO"
        );
    }

    estado.PTipos.pop();
    const int condCod = estado.PilaO.top();
    estado.PilaO.pop();
    const std::string condicion = numeroAOperandoRaw(condCod, estado);
    const int salto = generarSalto(estado, "gotofalso", condicion);
    GeneracionCodigo::liberarTemporalAvail(estado, condicion);
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "IF: gotofalso " + condicion +
        " pendiente en " + std::to_string(salto)
    );
}

void iniciarElse(EstadoSemantico& estado) {
    if (estado.PSaltos.empty()) {
        throw std::logic_error(
            "[SALTOS] No existe un gotofalso pendiente para ELSE"
        );
    }

    const int saltoFalso = estado.PSaltos.top();
    estado.PSaltos.pop();
    const int saltoFin = generarSalto(estado, "goto", "_");
    completarSalto(estado, saltoFalso, estado.CONT);
    estado.PSaltos.push(saltoFin);
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "ELSE: gotofalso -> " + std::to_string(estado.CONT) +
        ", goto pendiente en " + std::to_string(saltoFin)
    );
}

void finalizarIf(EstadoSemantico& estado) {
    if (estado.PSaltos.empty()) {
        throw std::logic_error(
            "[SALTOS] No existe un salto pendiente al finalizar IF"
        );
    }

    const int salto = estado.PSaltos.top();
    estado.PSaltos.pop();
    completarSalto(estado, salto, estado.CONT);
    GeneracionCodigo::registrarEstadoPilas(
        estado,
        "IF: salto " + std::to_string(salto) +
        " -> " + std::to_string(estado.CONT)
    );
}

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
    if (indiceCuadruplo < 1 ||
        indiceCuadruplo > static_cast<int>(estado.cuadruplos.size())) {
        throw std::out_of_range(
            "[SALTOS] Indice de cuadruplo invalido durante backpatching"
        );
    }
    const std::string destinoStr = std::to_string(destino);
    estado.cuadruplos[indiceCuadruplo - 1].res = destinoStr;
    estado.cuadruplos[indiceCuadruplo - 1].resRaw = destinoStr;
    estado.cuadruplos[indiceCuadruplo - 1].resNum = destino;
}

} // namespace ControlFlujo

// ============================================================================
// MODULO 7: INTERPRETE / EVALUADOR DE CUADRUPLOS
//
// Recorre los cuadruplos generados respetando el flujo de saltos condicionales
// (gotofalso) e incondicionales (goto) para fijar el valor real de las variables
// de acuerdo a la rama efectivamente ejecutada.
// ============================================================================

namespace Interprete {

void ejecutarCuadruplos(EstadoSemantico& estado) {
    if (estado.cuadruplos.empty()) {
        return;
    }

    // Mapeo de direccion RAM -> identificador de la variable
    std::map<int, std::string> ramAId;
    std::map<std::string, std::string> memoriaVariables;
    std::map<std::string, bool> variablesConocidas;
    std::map<std::string, std::string> memoriaTemporales;

    for (const auto& [id, simbolo] : estado.tablaSimbolos) {
        if (simbolo.direccionRAM >= 0) {
            ramAId[simbolo.direccionRAM] = id;
        }
        if (simbolo.esConstante) {
            memoriaVariables[id] = simbolo.valorConstante;
            variablesConocidas[id] = true;
        } else {
            variablesConocidas[id] = false;
        }
    }

    auto resolverValorPorCodigo = [&](int cod, const std::string& fallback) -> std::pair<bool, std::string> {
        if (cod == OPERANDO_NULO) {
            return {false, ""};
        }
        if (cod >= 0 && cod < BASE_TEMPORALES) {
            auto itRam = ramAId.find(cod);
            if (itRam != ramAId.end()) {
                const std::string& idVar = itRam->second;
                if (variablesConocidas[idVar]) {
                    return {true, memoriaVariables[idVar]};
                }
            }
            return {false, ""};
        }
        if (cod > BASE_TEMPORALES && cod < BASE_CONSTANTES) {
            std::string tempKey = "$T" + std::to_string(cod - BASE_TEMPORALES);
            auto itTemp = memoriaTemporales.find(tempKey);
            if (itTemp != memoriaTemporales.end()) {
                return {true, itTemp->second};
            }
            return {false, ""};
        }
        if (cod > BASE_CONSTANTES) {
            auto itLit = estado.tablaLiterales.find(cod);
            if (itLit != estado.tablaLiterales.end()) {
                return {true, itLit->second};
            }
        }
        if (fallback == "true" || fallback == "false") {
            return {true, fallback};
        }
        try {
            std::stod(fallback);
            return {true, fallback};
        } catch (...) {
            if ((fallback.size() >= 2 && fallback.front() == '"' && fallback.back() == '"') ||
                (fallback.size() >= 2 && fallback.front() == '\'' && fallback.back() == '\'')) {
                return {true, fallback};
            }
        }
        return {false, ""};
    };

    const int totalCuadruplos = static_cast<int>(estado.cuadruplos.size());
    int pc = 1;
    int pasos = 0;
    const int maxPasos = 50000;

    while (pc >= 1 && pc <= totalCuadruplos && pasos++ < maxPasos) {
        const auto& c = estado.cuadruplos[pc - 1];
        const int opNum = c.coperNum;

        if (opNum == OP_NUM_ASIG || c.coper == "=") {
            auto [conocido, val] = resolverValorPorCodigo(c.op1Num, c.op1);
            std::string destino;
            if (c.resNum >= 0 && c.resNum < BASE_TEMPORALES && ramAId.count(c.resNum)) {
                destino = ramAId[c.resNum];
            } else {
                destino = !c.resRaw.empty() ? c.resRaw : c.res;
            }

            if (conocido) {
                memoriaVariables[destino] = val;
                variablesConocidas[destino] = true;
            } else {
                memoriaVariables.erase(destino);
                variablesConocidas[destino] = false;
            }
            pc++;
        }
        else if (opNum == OP_NUM_GOTOFALSO || c.coper == "gotofalso") {
            auto [conocido, cond] = resolverValorPorCodigo(c.op1Num, c.op1);
            int destino = pc + 1;
            if (c.resNum > 0) {
                destino = c.resNum;
            } else {
                try { destino = std::stoi(c.res); } catch (...) { destino = pc + 1; }
            }

            if (conocido && (cond == "false" || cond == "0")) {
                pc = destino;
            } else {
                pc++;
            }
        }
        else if (opNum == OP_NUM_GOTO || c.coper == "goto") {
            int destino = pc + 1;
            if (c.resNum > 0) {
                destino = c.resNum;
            } else {
                try { destino = std::stoi(c.res); } catch (...) { destino = pc + 1; }
            }
            pc = destino;
        }
        else if (opNum == OP_NUM_INPUT || c.coper == "input") {
            std::string destino;
            if (c.resNum >= 0 && c.resNum < BASE_TEMPORALES && ramAId.count(c.resNum)) {
                destino = ramAId[c.resNum];
            } else {
                destino = !c.resRaw.empty() ? c.resRaw : c.res;
            }
            variablesConocidas[destino] = false;
            memoriaVariables.erase(destino);
            pc++;
        }
        else if (opNum == OP_NUM_OUTPUT || c.coper == "output") {
            pc++;
        }
        else if (opNum == OP_NUM_NOT || opNum == OP_NUM_NEG || c.coper == "!" || c.coper == "NEG") {
            auto [conocido, val] = resolverValorPorCodigo(c.op1Num, c.op1);
            std::string temporal = !c.resRaw.empty() ? c.resRaw : c.res;
            if (c.resNum > BASE_TEMPORALES && c.resNum < BASE_CONSTANTES) {
                temporal = "$T" + std::to_string(c.resNum - BASE_TEMPORALES);
            }
            if (conocido) {
                if (opNum == OP_NUM_NOT || c.coper == "!") {
                    memoriaTemporales[temporal] = (val == "true") ? "false" : "true";
                } else {
                    try {
                        double num = std::stod(val);
                        memoriaTemporales[temporal] = formatearNumero(-num, val.find('.') == std::string::npos);
                    } catch (...) {
                        memoriaTemporales.erase(temporal);
                    }
                }
            } else {
                memoriaTemporales.erase(temporal);
            }
            pc++;
        }
        else {
            // Operación binaria
            auto [c1, v1] = resolverValorPorCodigo(c.op1Num, c.op1);
            auto [c2, v2] = resolverValorPorCodigo(c.op2Num, c.op2);
            std::string temporal = !c.resRaw.empty() ? c.resRaw : c.res;
            if (c.resNum > BASE_TEMPORALES && c.resNum < BASE_CONSTANTES) {
                temporal = "$T" + std::to_string(c.resNum - BASE_TEMPORALES);
            }
            if (c1 && c2) {
                std::string resVal = calcularValor(c.coper, v1, v2);
                if (!resVal.empty()) {
                    memoriaTemporales[temporal] = resVal;
                } else {
                    memoriaTemporales.erase(temporal);
                }
            } else {
                memoriaTemporales.erase(temporal);
            }
            pc++;
        }
    }

    // Sincronizar los valores finales con la tabla de símbolos
    for (auto& [id, simbolo] : estado.tablaSimbolos) {
        if (simbolo.esConstante) {
            continue;
        }
        if (variablesConocidas[id]) {
            simbolo.valorConocido = true;
            simbolo.valorActual = memoriaVariables[id];
            simbolo.inicializada = true;
        } else {
            simbolo.valorConocido = false;
            simbolo.valorActual.clear();
        }
    }
}

} // namespace Interprete

// ============================================================================
// LIMPIEZA DEL ESTADO
// ============================================================================

void reiniciarEstado(EstadoSemantico& estado) {
    // Cada pulsacion de Analizar debe iniciar desde un estado limpio. Si no
    // se vaciaran las pilas, los simbolos y CONT de una ejecucion anterior
    // contaminarian la siguiente.
    estado.tablaSimbolos.clear();
    estado.cuadruplos.clear();
    estado.historialPilas.clear();
    estado.valoresTemporales.clear();
    estado.identificadoresPendientes.clear();
    estado.idDestinoAsignacion.clear();
    estado.tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
    estado.siguienteDireccionRAM = 0;
    estado.siguienteTemporal = 21;
    estado.CONT = 1;
    estado.tablaLiterales.clear();
    estado.literalAId.clear();
    estado.siguienteLiteral = BASE_CONSTANTES + 1;

    while (!estado.PilaO.empty()) estado.PilaO.pop();
    while (!estado.PTipos.empty()) estado.PTipos.pop();
    while (!estado.POper.empty()) estado.POper.pop();
    while (!estado.PSaltos.empty()) estado.PSaltos.pop();
    while (!estado.temporalesDisponibles.empty()) {
        estado.temporalesDisponibles.pop();
    }
    for (int i = 20; i >= 1; --i) {
        estado.temporalesDisponibles.push(BASE_TEMPORALES + i);
    }
}

} // namespace Semantica
