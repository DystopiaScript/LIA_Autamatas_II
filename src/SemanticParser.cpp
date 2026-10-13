#include "SemanticParser.h"

#include <stdexcept>

namespace Semantica {

namespace {

bool esNumerico(TipoDato tipo) {
    return tipo == TipoDato::ENTERO || tipo == TipoDato::REAL;
}

bool esOperadorRelacional(const std::string& operador) {
    return operador == "==" || operador == "!=" ||
           operador == "<"  || operador == "<=" ||
           operador == ">"  || operador == ">=";
}

void exigirPilasAlineadas(const EstadoSemantico& estado) {
    if (estado.PilaO.size() != estado.PTipos.size()) {
        throw std::logic_error(
            "Estado semantico invalido: PilaO y PTipos no estan alineadas"
        );
    }
}

} // namespace

std::string tipoToString(TipoDato tipo) {
    switch (tipo) {
        case TipoDato::ENTERO:   return "int";
        case TipoDato::REAL:     return "float";
        case TipoDato::CARACTER: return "char";
        case TipoDato::BOOLEANO: return "bool";
        case TipoDato::CADENA:   return "string";
        case TipoDato::ERROR_SEMANTICO:
        default:                 return "ERROR_SEMANTICO";
    }
}

std::string obtenerTemporalAvail(EstadoSemantico& estado) {
    return "$T" + std::to_string(estado.siguienteTemporal++);
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
        if (esNumerico(t1) && esNumerico(t2)) {
            return t1 == TipoDato::REAL || t2 == TipoDato::REAL
                ? TipoDato::REAL
                : TipoDato::ENTERO;
        }
        return TipoDato::ERROR_SEMANTICO;
    }

    if (operador == "%") {
        return t1 == TipoDato::ENTERO && t2 == TipoDato::ENTERO
            ? TipoDato::ENTERO
            : TipoDato::ERROR_SEMANTICO;
    }

    if (operador == "&&" || operador == "||") {
        return t1 == TipoDato::BOOLEANO && t2 == TipoDato::BOOLEANO
            ? TipoDato::BOOLEANO
            : TipoDato::ERROR_SEMANTICO;
    }

    if (esOperadorRelacional(operador)) {
        if (esNumerico(t1) && esNumerico(t2)) {
            return TipoDato::BOOLEANO;
        }

        if ((operador == "==" || operador == "!=") && t1 == t2) {
            return TipoDato::BOOLEANO;
        }

        return TipoDato::ERROR_SEMANTICO;
    }

    return TipoDato::ERROR_SEMANTICO;
}

bool tiposCompatiblesAsignacion(TipoDato destino, TipoDato fuente) {
    if (destino == fuente) {
        return true;
    }

    // Conversión implícita permitida: entero a real.
    return destino == TipoDato::REAL && fuente == TipoDato::ENTERO;
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

void registrarSimbolo(
    EstadoSemantico& estado,
    const std::string& id,
    TipoDato tipo
) {
    if (id.empty()) {
        throw std::invalid_argument(
            "No se puede registrar un identificador vacio"
        );
    }

    if (tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "No se puede registrar un identificador con tipo invalido"
        );
    }

    if (estado.tablaSimbolos.find(id) != estado.tablaSimbolos.end()) {
        throw std::runtime_error(
            "Identificador redeclarado: " + id
        );
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
    const std::map<std::string, Simbolo>::const_iterator encontrado =
        estado.tablaSimbolos.find(id);

    if (encontrado == estado.tablaSimbolos.end()) {
        throw std::runtime_error(
            "Identificador no declarado: " + id
        );
    }

    return encontrado->second;
}

int precedencia(const std::string& operador) {
    if (operador == "||") return 1;
    if (operador == "&&") return 2;
    if (esOperadorRelacional(operador)) return 3;
    if (operador == "+" || operador == "-") return 4;
    if (operador == "*" || operador == "/" || operador == "%") return 5;
    return -1;
}

void reducirOperador(EstadoSemantico& estado) {
    if (estado.POper.empty()) {
        throw std::logic_error(
            "No hay operador pendiente para reducir"
        );
    }

    if (estado.POper.top() == "(") {
        throw std::logic_error(
            "No se puede reducir la marca de parentesis"
        );
    }

    accion5GenerarOperacion(estado);
}

void recibirOperador(
    EstadoSemantico& estado,
    const std::string& operador
) {
    if (precedencia(operador) < 0) {
        throw std::invalid_argument(
            "Operador no reconocido: " + operador
        );
    }

    while (!estado.POper.empty() &&
           estado.POper.top() != "(" &&
           precedencia(estado.POper.top()) >= precedencia(operador)) {
        reducirOperador(estado);
    }

    estado.POper.push(operador);
}

void accion1AcumularIdentificador(
    EstadoSemantico& estado,
    const std::string& id
) {
    estado.identificadoresPendientes.push_back(id);
}

void accion1RegistrarDeclaracion(
    EstadoSemantico& estado,
    TipoDato tipo
) {
    for (const std::string& id : estado.identificadoresPendientes) {
        registrarSimbolo(estado, id, tipo);
    }

    estado.identificadoresPendientes.clear();
}

void accion2GuardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
) {
    recibirOperador(estado, operador);
}

void accion3GuardarOperador(
    EstadoSemantico& estado,
    const std::string& operador
) {
    recibirOperador(estado, operador);
}

void accion4ReducirOperador(EstadoSemantico& estado) {
    reducirOperador(estado);
}

void accion5GenerarOperacion(EstadoSemantico& estado) {
    exigirPilasAlineadas(estado);

    if (estado.POper.empty() || estado.POper.top() == "(") {
        throw std::logic_error(
            "No hay un operador valido para generar la operacion"
        );
    }

    if (estado.PilaO.size() < 2 || estado.PTipos.size() < 2) {
        throw std::logic_error(
            "Faltan operandos para generar la operacion"
        );
    }

    const std::string operador = estado.POper.top();
    estado.POper.pop();

    const TipoDato tipoDerecho = estado.PTipos.top();
    estado.PTipos.pop();
    const TipoDato tipoIzquierdo = estado.PTipos.top();
    estado.PTipos.pop();

    const std::string operandoDerecho = estado.PilaO.top();
    estado.PilaO.pop();
    const std::string operandoIzquierdo = estado.PilaO.top();
    estado.PilaO.pop();

    const TipoDato tipoResultado =
        CuboSemantico(tipoIzquierdo, operador, tipoDerecho);

    if (tipoResultado == TipoDato::ERROR_SEMANTICO) {
        throw std::runtime_error(
            "Tipos incompatibles para el operador '" + operador + "'"
        );
    }

    const std::string temporal = obtenerTemporalAvail(estado);

    generarCuadruplo(
        estado,
        operador,
        operandoIzquierdo,
        operandoDerecho,
        temporal
    );

    estado.PilaO.push(temporal);
    estado.PTipos.push(tipoResultado);
}

void accion6AbrirParentesis(EstadoSemantico& estado) {
    estado.POper.push("(");
}

void accion7CerrarParentesis(EstadoSemantico& estado) {
    while (!estado.POper.empty() && estado.POper.top() != "(") {
        reducirOperador(estado);
    }

    if (estado.POper.empty()) {
        throw std::logic_error(
            "Parentesis de cierre sin marca de apertura"
        );
    }

    estado.POper.pop();
}

void apilarIdentificador(
    EstadoSemantico& estado,
    const std::string& id
) {
    exigirPilasAlineadas(estado);
    const Simbolo& simbolo = buscarSimbolo(estado, id);
    estado.PilaO.push(id);
    estado.PTipos.push(simbolo.tipo);
    estado.contexto = ContextoSemantico::FACTOR;
}

void apilarConstante(
    EstadoSemantico& estado,
    const std::string& valor,
    TipoDato tipo
) {
    if (tipo == TipoDato::ERROR_SEMANTICO) {
        throw std::invalid_argument(
            "Constante con tipo invalido"
        );
    }

    exigirPilasAlineadas(estado);
    estado.PilaO.push(valor);
    estado.PTipos.push(tipo);
    estado.contexto = ContextoSemantico::FACTOR;
}

void iniciarAsignacion(
    EstadoSemantico& estado,
    const std::string& idDestino
) {
    const Simbolo& destino = buscarSimbolo(estado, idDestino);
    estado.idDestinoAsignacion = idDestino;
    estado.tipoDestinoAsignacion = destino.tipo;
    estado.contexto = ContextoSemantico::ASIGNACION;
}

void finalizarAsignacion(EstadoSemantico& estado) {
    exigirPilasAlineadas(estado);

    if (estado.PilaO.empty()) {
        throw std::logic_error(
            "La asignacion no tiene resultado de expresion"
        );
    }

    const std::string expresion = estado.PilaO.top();
    estado.PilaO.pop();
    const TipoDato tipoExpresion = estado.PTipos.top();
    estado.PTipos.pop();

    if (!tiposCompatiblesAsignacion(
            estado.tipoDestinoAsignacion,
            tipoExpresion)) {
        throw std::runtime_error(
            "Incompatibilidad de tipos en la asignacion de '" +
            estado.idDestinoAsignacion + "'"
        );
    }

    generarCuadruplo(
        estado,
        ":=",
        expresion,
        "_",
        estado.idDestinoAsignacion
    );

    estado.tablaSimbolos.at(
        estado.idDestinoAsignacion
    ).inicializada = true;

    estado.idDestinoAsignacion.clear();
    estado.tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
    estado.contexto = ContextoSemantico::NINGUNO;
}

void reiniciarEstado(EstadoSemantico& estado) {
    estado.tablaSimbolos.clear();

    while (!estado.PilaO.empty()) estado.PilaO.pop();
    while (!estado.PTipos.empty()) estado.PTipos.pop();
    while (!estado.POper.empty()) estado.POper.pop();
    while (!estado.PSaltos.empty()) estado.PSaltos.pop();

    estado.cuadruplos.clear();
    estado.identificadoresPendientes.clear();
    estado.idDestinoAsignacion.clear();
    estado.tipoDestinoAsignacion = TipoDato::ERROR_SEMANTICO;
    estado.contexto = ContextoSemantico::NINGUNO;
    estado.siguienteDireccionRAM = 0;
    estado.siguienteTemporal = 1;
    estado.CONT = 0;
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

    estado.cuadruplos[indiceCuadruplo].res =
        std::to_string(destino);
}

} // namespace Semantica
