#include "SemanticAnalysis.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

using Semantica::EstadoSemantico;
using Semantica::TipoDato;

bool contiene(
    const std::vector<std::string>& valores,
    const std::string& valor
) {
    for (const std::string& actual : valores) {
        if (actual == valor) return true;
    }
    return false;
}

std::string pilaTexto(std::stack<std::string> pila) {
    std::vector<std::string> valores;
    while (!pila.empty()) {
        valores.push_back(pila.top());
        pila.pop();
    }
    std::ostringstream salida;
    for (auto it = valores.rbegin(); it != valores.rend(); ++it) {
        salida << *it << ' ';
    }
    return salida.str();
}

std::string tiposTexto(std::stack<TipoDato> pila) {
    std::vector<TipoDato> valores;
    while (!pila.empty()) {
        valores.push_back(pila.top());
        pila.pop();
    }
    std::ostringstream salida;
    for (auto it = valores.rbegin(); it != valores.rend(); ++it) {
        salida << Semantica::tipoToString(*it) << ' ';
    }
    return salida.str();
}

} // namespace

SemanticAnalysisResult SemanticAnalysis::analizar(
    const std::vector<Token>& tokens
) const {
    SemanticAnalysisResult resultado;
    Semantica::reiniciarEstado(resultado.estado);

    try {
        std::size_t posicion = 0;
        while (posicion < tokens.size()) {
            const std::string& lexema = tokens[posicion].lexema;

            if (lexema == "def") {
                Semantica::Declaraciones::iniciarDeclaracion(
                    resultado.estado
                );
                ++posicion;
                while (posicion < tokens.size() &&
                       tokens[posicion].lexema != ":") {
                    if (tokens[posicion].gramema == IDENTIFICADOR) {
                        Semantica::Declaraciones::acumularIdentificador(
                            resultado.estado,
                            tokens[posicion].lexema
                        );
                    }
                    ++posicion;
                }
                if (posicion >= tokens.size() ||
                    tokens[posicion].lexema != ":") {
                    throw std::runtime_error(
                        "[DECLARACIONES] Falta ':' en la declaracion"
                    );
                }
                ++posicion;
                if (posicion >= tokens.size() ||
                    !esTipo(tokens[posicion].lexema)) {
                    throw std::runtime_error(
                        "[DECLARACIONES] Falta tipo de declaracion"
                    );
                }
                const TipoDato tipo =
                    tipoDesdeLexema(tokens[posicion].lexema);
                Semantica::Declaraciones::declararVariables(
                    resultado.estado,
                    tipo
                );
                while (posicion < tokens.size() &&
                       tokens[posicion].lexema != ";") {
                    ++posicion;
                }
                if (posicion < tokens.size()) ++posicion;
                continue;
            }

            if (lexema == "input" || lexema == "output") {
                const bool esEntrada = lexema == "input";
                ++posicion;
                if (posicion >= tokens.size() ||
                    tokens[posicion].lexema != "(") {
                    throw std::runtime_error(
                        "[ENTRADA/SALIDA] Falta '('"
                    );
                }
                ++posicion;
                while (posicion < tokens.size() &&
                       tokens[posicion].lexema != ")") {
                    if (esEntrada) {
                        const std::size_t fin = procesarExpresion(
                            tokens,
                            posicion,
                            {",", ")"},
                            resultado.estado
                        );
                        const std::string expresion =
                            Semantica::Expresiones::obtenerResultado(
                                resultado.estado
                            );
                        Semantica::EntradaSalida::generarSalida(
                            resultado.estado,
                            expresion
                        );
                        posicion = fin;
                    } else {
                        if (tokens[posicion].gramema != IDENTIFICADOR) {
                            throw std::runtime_error(
                                "[ENTRADA] Se esperaba un identificador"
                            );
                        }
                        Semantica::EntradaSalida::generarEntrada(
                            resultado.estado,
                            tokens[posicion].lexema
                        );
                        ++posicion;
                    }
                    if (posicion < tokens.size() &&
                        tokens[posicion].lexema == ",") {
                        ++posicion;
                    }
                }
                if (posicion < tokens.size() &&
                    tokens[posicion].lexema == ")") {
                    ++posicion;
                }
                while (posicion < tokens.size() &&
                       tokens[posicion].lexema != ";") {
                    ++posicion;
                }
                if (posicion < tokens.size()) ++posicion;
                continue;
            }

            if (tokens[posicion].gramema == IDENTIFICADOR &&
                posicion + 1 < tokens.size() &&
                tokens[posicion + 1].lexema == "=") {
                const std::string destino = tokens[posicion].lexema;
                Semantica::VerificacionSemantica::iniciarAsignacion(
                    resultado.estado,
                    destino
                );
                posicion += 2;
                posicion = procesarExpresion(
                    tokens,
                    posicion,
                    {";"},
                    resultado.estado
                );
                Semantica::Expresiones::finalizarAsignacion(
                    resultado.estado
                );
                if (posicion < tokens.size()) ++posicion;
                continue;
            }

            ++posicion;
        }
        resultado.correcto = resultado.errores.empty();
    } catch (const std::exception& error) {
        resultado.errores.push_back(error.what());
        resultado.correcto = false;
    }

    return resultado;
}

std::size_t SemanticAnalysis::procesarExpresion(
    const std::vector<Token>& tokens,
    std::size_t inicio,
    const std::vector<std::string>& terminadores,
    EstadoSemantico& estado
) {
    std::size_t posicion = inicio;
    bool esperaOperando = true;
    int profundidadParentesis = 0;
    while (posicion < tokens.size() &&
           !(profundidadParentesis == 0 &&
             contiene(terminadores, tokens[posicion].lexema))) {
        const Token& token = tokens[posicion];
        const std::string& lexema = token.lexema;

        if (lexema == "(") {
            Semantica::Expresiones::abrirParentesis(estado);
            ++profundidadParentesis;
            ++posicion;
            esperaOperando = true;
        } else if (lexema == ")") {
            Semantica::Expresiones::cerrarParentesis(estado);
            --profundidadParentesis;
            ++posicion;
            esperaOperando = false;
        } else if (token.gramema == IDENTIFICADOR) {
            Semantica::Expresiones::apilarIdentificador(
                estado,
                lexema
            );
            ++posicion;
            esperaOperando = false;
        } else if (token.gramema == CONSTANTE_ENTERA ||
                   token.gramema == CONSTANTE_FLOTANTE ||
                   token.gramema == CONSTANTE_CIENTIFICA ||
                   token.gramema == CONSTANTE_CARACTER ||
                   token.gramema == CONSTANTE_STRING) {
            TipoDato tipo = TipoDato::CADENA;
            if (token.gramema == CONSTANTE_ENTERA) {
                tipo = TipoDato::ENTERO;
            } else if (token.gramema == CONSTANTE_FLOTANTE ||
                       token.gramema == CONSTANTE_CIENTIFICA) {
                tipo = TipoDato::REAL;
            } else if (token.gramema == CONSTANTE_CARACTER) {
                tipo = TipoDato::CARACTER;
            }
            Semantica::Expresiones::apilarConstante(
                estado,
                lexema,
                tipo
            );
            ++posicion;
            esperaOperando = false;
        } else if (esOperador(token)) {
            const std::string op = operador(token);
            if (esperaOperando &&
                (op == "-" || op == "!")) {
                Semantica::Expresiones::recibirOperadorUnario(
                    estado,
                    op
                );
            } else {
                while (!estado.POper.empty() &&
                       estado.POper.top() != "(" &&
                       Semantica::GeneracionCodigo::precedencia(
                           estado.POper.top()
                       ) >= Semantica::GeneracionCodigo::precedencia(op)) {
                    const std::string anterior = estado.POper.top();
                    estado.POper.pop();
                    if (anterior == "!" || anterior == "NEG") {
                        Semantica::GeneracionCodigo::reducirOperacionUnaria(
                            estado,
                            anterior
                        );
                    } else {
                        Semantica::GeneracionCodigo::reducirOperacion(
                            estado,
                            anterior
                        );
                    }
                }
                Semantica::Expresiones::recibirOperador(
                    estado,
                    op
                );
            }
            ++posicion;
            esperaOperando = true;
        } else {
            throw std::runtime_error(
                "[EXPRESIONES] Token inesperado: " + lexema
            );
        }
    }

    Semantica::Expresiones::finalizarOperacionPendiente(estado);
    return posicion;
}

TipoDato SemanticAnalysis::tipoDesdeLexema(const std::string& lexema) {
    if (lexema == "int") return TipoDato::ENTERO;
    if (lexema == "float") return TipoDato::REAL;
    if (lexema == "char") return TipoDato::CARACTER;
    if (lexema == "bool") return TipoDato::BOOLEANO;
    if (lexema == "string") return TipoDato::CADENA;
    return TipoDato::ERROR_SEMANTICO;
}

bool SemanticAnalysis::esTipo(const std::string& lexema) {
    return tipoDesdeLexema(lexema) != TipoDato::ERROR_SEMANTICO;
}

bool SemanticAnalysis::esOperador(const Token& token) {
    return token.gramema == OP_SUMA ||
           token.gramema == OP_RESTA ||
           token.gramema == OP_MULTIPLICACION ||
           token.gramema == OP_DIVISION ||
           token.gramema == OP_MODULO ||
           token.gramema == OP_IGUALDAD ||
           token.gramema == OP_DIFERENTE ||
           token.gramema == OP_MENOR ||
           token.gramema == OP_MENOR_IGUAL ||
           token.gramema == OP_MAYOR ||
           token.gramema == OP_MAYOR_IGUAL ||
           token.gramema == OP_AND ||
           token.gramema == OP_OR ||
           token.gramema == OP_NOT;
}

std::string SemanticAnalysis::operador(const Token& token) {
    return token.lexema;
}

std::string SemanticAnalysis::tablaSimbolos(
    const EstadoSemantico& estado
) {
    std::ostringstream salida;
    salida << std::left
           << std::setw(16) << "ID"
           << std::setw(12) << "TIPO"
           << std::setw(10) << "RAM"
           << std::setw(14) << "CONSTANTE"
           << "VALOR\n";
    salida << std::string(65, '-') << '\n';
    for (const auto& entrada : estado.tablaSimbolos) {
        const auto& simbolo = entrada.second;
        salida << std::left
               << std::setw(16) << simbolo.id
               << std::setw(12) << Semantica::tipoToString(simbolo.tipo)
               << std::setw(10) << simbolo.direccionRAM
               << std::setw(14) << (simbolo.esConstante ? "SI" : "NO")
               << simbolo.valorConstante << '\n';
    }
    return salida.str();
}

std::string SemanticAnalysis::pilas(const EstadoSemantico& estado) {
    std::ostringstream salida;
    salida << "PilaO   (base -> tope): "
           << pilaTexto(estado.PilaO) << '\n';
    salida << "PTipos  (base -> tope): "
           << tiposTexto(estado.PTipos) << '\n';
    salida << "POper   (base -> tope): "
           << pilaTexto(estado.POper) << '\n';
    salida << "PSaltos (base -> tope): ";
    std::stack<int> saltos = estado.PSaltos;
    std::vector<int> valores;
    while (!saltos.empty()) {
        valores.push_back(saltos.top());
        saltos.pop();
    }
    for (auto it = valores.rbegin(); it != valores.rend(); ++it) {
        salida << *it << ' ';
    }
    salida << '\n';
    salida << "AVAIL  (temporales): "
           << pilaTexto(estado.temporalesDisponibles) << '\n';
    return salida.str();
}

std::string SemanticAnalysis::cuadruplos(
    const EstadoSemantico& estado
) {
    std::ostringstream salida;
    salida << std::left
           << std::setw(8) << "CONT"
           << std::setw(14) << "COPER"
           << std::setw(16) << "OP1"
           << std::setw(16) << "OP2"
           << "RES\n";
    salida << std::string(70, '-') << '\n';
    for (std::size_t i = 0; i < estado.cuadruplos.size(); ++i) {
        const auto& cuadruplo = estado.cuadruplos[i];
        salida << std::left
               << std::setw(8) << (i + 1)
               << std::setw(14) << cuadruplo.coper
               << std::setw(16) << cuadruplo.op1
               << std::setw(16) << cuadruplo.op2
               << cuadruplo.res << '\n';
    }
    return salida.str();
}
