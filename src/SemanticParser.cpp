#include "SemanticParser.h"

#include <stdexcept>

namespace Semantica {

namespace {

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
            "PilaO y PTipos deben contener la misma cantidad de elementos"
        );
    }
}

} // namespace

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

namespace VerificacionSemantica {

void registrarSimbolo(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo
) {
    if (id.empty() || tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument("Identificador o tipo invalido");
    }

    if (estado.tablaSimbolos.find(id) != estado.tablaSimbolos.end()) {
        throw std::runtime_error("Identificador redeclarado: " + id);
    }

    Simbolo simbolo;
    simbolo.id = id;
    simbolo.tipo = tipo;
    simbolo.direccionRAM = estado.siguienteDireccionRAM++;
    estado.tablaSimbolos.emplace(id, simbolo);
}

const Simbolo& buscarSimbolo(
    const EstadoSemantico& estado,
    const std::string& id
) {
    const auto it = estado.tablaSimbolos.find(id);
    if (it == estado.tablaSimbolos.end()) {
        throw std::runtime_error("Identificador no declarado: " + id);
    }
    return it->second;
}

void apilarTipo(EstadoSemantico& estado, TipoDato tipo) {
    if (tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument("No se puede apilar un tipo invalido");
    }
    estado.PTipos.push(tipo);
}

TipoDato CuboSemantico(
    TipoDato t1,
    const std::string& operador,
    TipoDato t2
) {
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
        throw std::logic_error("Faltan tipos para validar la operacion");
    }

    const TipoDato t2 = estado.PTipos.top();
    estado.PTipos.pop();
    const TipoDato t1 = estado.PTipos.top();
    estado.PTipos.pop();
    const TipoDato resultado = CuboSemantico(t1, operador, t2);

    if (resultado == TipoDato::ERROR_SEMANTICO) {
        throw std::runtime_error(
            "Tipos incompatibles para el operador '" + operador + "'"
        );
    }

    estado.PTipos.push(resultado);
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
            "Incompatibilidad de tipos en la asignacion de '" +
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

namespace GeneracionCodigo {

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
        throw std::invalid_argument("Operador no reconocido: " + operador);
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
        reducirOperacion(estado, operador);
    }

    if (estado.POper.empty()) {
        throw std::logic_error(
            "Parentesis de cierre sin marca de apertura"
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
    exigirPilasAlineadas(estado);
    if (estado.PilaO.size() < 2) {
        throw std::logic_error("Faltan operandos para reducir");
    }

    const std::string op2 = estado.PilaO.top();
    estado.PilaO.pop();
    const std::string op1 = estado.PilaO.top();
    estado.PilaO.pop();

    VerificacionSemantica::validarOperacion(estado, operador);

    const std::string temporal = obtenerTemporalAvail(estado);
    generarCuadruplo(estado, operador, op1, op2, temporal);
    liberarTemporalAvail(estado, op1);
    liberarTemporalAvail(estado, op2);
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
    return -1;
}

} // namespace GeneracionCodigo

namespace ControlFlujo {

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
            "Indice de cuadruplo invalido durante backpatching"
        );
    }
    estado.cuadruplos[indiceCuadruplo].res = std::to_string(destino);
}

} // namespace ControlFlujo

void reiniciarEstado(EstadoSemantico& estado) {
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
