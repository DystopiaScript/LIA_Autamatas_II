#include "SemanticAnalysis.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

// Este archivo contiene un analizador semántico auxiliar que trabaja sobre
// una lista completa de tokens. El flujo principal de la aplicación usa las
// acciones semánticas embebidas en Parser.cpp, pero estas funciones siguen
// siendo útiles para pruebas, compatibilidad y formateo textual del estado.

namespace {

using Semantica::EstadoSemantico;
using Semantica::TipoDato;

// Indica si una cadena aparece dentro de una lista de terminadores.
// procesarExpresion usa esta función para saber dónde termina una expresión:
// por ejemplo, en ';', ',' o ')' según el contexto.
bool contiene(
    const std::vector<std::string>& valores,
    const std::string& valor
) {
    for (const std::string& actual : valores) {
        if (actual == valor) return true;
    }
    return false;
}

// Convierte una copia de una pila de cadenas a texto legible.
//
// La pila se copia por valor porque para imprimirla hay que hacer pop de sus
// elementos. El resultado se reconstruye de base a tope, que es el orden
// didáctico utilizado por la interfaz.
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

// Igual que pilaTexto(), pero convierte cada TipoDato a su nombre textual.
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
    // Resultado acumulado que contiene:
    // - estado semántico: símbolos, pilas y cuádruplos;
    // - errores encontrados;
    // - indicador final de éxito.
    SemanticAnalysisResult resultado;

    // Cada análisis debe comenzar limpio para no mezclar declaraciones,
    // temporales o cuádruplos de una ejecución anterior.
    Semantica::reiniciarEstado(resultado.estado);

    try {
        std::size_t posicion = 0;
        while (posicion < tokens.size()) {
            const std::string& lexema = tokens[posicion].lexema;

            // Reconocimiento simplificado de declaraciones:
            //     def id [, id] : tipo ;
            //
            // Se acumulan los identificadores hasta encontrar ':'. Después
            // se convierte el lexema del tipo y se registran todos juntos.
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

            // Entrada y salida:
            //     input  ( id, ... ) ;
            //     output ( expr, ... ) ;
            //
            // En esta capa auxiliar las dos formas se distinguen con
            // esEntrada. El parser principal realiza esta misma tarea con
            // sus producciones LL(1) y acciones embebidas.
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
                        // Cada expresión de input se evalúa y se convierte
                        // en un cuádruplo de salida en esta implementación
                        // auxiliar. El flujo principal usa input para
                        // destinos identificadores; esta diferencia se
                        // conserva por compatibilidad histórica.
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
                        // La rama complementaria valida el identificador y
                        // genera la operación de entrada.
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

            // Reconocimiento de una asignación:
            //     identificador = expresión ;
            //
            // Primero se registra el destino. Después se procesa la
            // expresión derecha y finalmente se valida/genera la asignación.
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

        // Si no se lanzó una excepción, el análisis es correcto únicamente
        // cuando tampoco se acumularon errores.
        resultado.correcto = resultado.errores.empty();
    } catch (const std::exception& error) {
        // Las acciones semánticas reportan errores mediante excepciones para
        // detener la operación inválida sin generar código inconsistente.
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
    // Recorre una expresión hasta encontrar uno de sus terminadores en nivel
    // cero. Esto permite procesar expresiones dentro de listas y conservar
    // la coma o el punto y coma para el llamador.
    std::size_t posicion = inicio;

    // esperaOperando distingue entre una posición donde puede aparecer un
    // operando y una posición donde se espera un operador. Es necesaria para
    // reconocer '-' y '!' como operadores unarios.
    bool esperaOperando = true;

    // Los terminadores dentro de paréntesis no deben cerrar la expresión
    // exterior; por eso sólo se consideran cuando la profundidad es cero.
    int profundidadParentesis = 0;
    while (posicion < tokens.size() &&
           !(profundidadParentesis == 0 &&
             contiene(terminadores, tokens[posicion].lexema))) {
        const Token& token = tokens[posicion];
        const std::string& lexema = token.lexema;

        if (lexema == "(") {
            // La marca '(' evita reducir operadores que pertenecen a una
            // expresión exterior antes de cerrar el grupo actual.
            Semantica::Expresiones::abrirParentesis(estado);
            ++profundidadParentesis;
            ++posicion;
            esperaOperando = true;
        } else if (lexema == ")") {
            // Al cerrar, se reducen los operadores pendientes hasta la marca
            // de apertura y se elimina dicha marca.
            Semantica::Expresiones::cerrarParentesis(estado);
            --profundidadParentesis;
            ++posicion;
            esperaOperando = false;
        } else if (token.gramema == IDENTIFICADOR) {
            // Un identificador se busca en la tabla de símbolos y se apila
            // junto con su tipo.
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
            // Los literales no necesitan estar declarados. Aquí se determina
            // su TipoDato y se apilan como operandos de la expresión.
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

            // '-' al inicio de una expresión y '!' antes de un operando son
            // unarios: -x y !x. En cualquier otro caso el operador es binario.
            if (esperaOperando &&
                (op == "-" || op == "!")) {
                Semantica::Expresiones::recibirOperadorUnario(
                    estado,
                    op
                );
            } else {
                // Antes de guardar el operador actual, se reducen los que
                // tienen mayor o igual precedencia. Esto implementa la
                // precedencia usual de *, +, relacionales y lógicos.
                while (!estado.POper.empty() &&
                       estado.POper.top() != "(" &&
                       Semantica::GeneracionCodigo::precedencia(
                           estado.POper.top()
                       ) >= Semantica::GeneracionCodigo::precedencia(op)) {
                    const std::string anterior = estado.POper.top();
                    estado.POper.pop();

                    // Los operadores unarios consumen un solo operando;
                    // los demás consumen dos.
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
            // Cualquier token que no sea paréntesis, operando u operador
            // indica que la expresión no tiene una forma válida.
            throw std::runtime_error(
                "[EXPRESIONES] Token inesperado: " + lexema
            );
        }
    }

    // La expresión puede terminar por un terminador, así que aquí se vacían
    // los operadores que aún quedan pendientes en POper.
    Semantica::Expresiones::finalizarOperacionPendiente(estado);
    return posicion;
}

TipoDato SemanticAnalysis::tipoDesdeLexema(const std::string& lexema) {
    // Traduce los nombres escritos en el programa a la enumeración interna.
    // "real" es un alias aceptado para el tipo REAL.
    if (lexema == "int") return TipoDato::ENTERO;
    if (lexema == "float" || lexema == "real") {
        return TipoDato::REAL;
    }
    if (lexema == "char") return TipoDato::CARACTER;
    if (lexema == "bool") return TipoDato::BOOLEANO;
    if (lexema == "string") return TipoDato::CADENA;
    return TipoDato::ERROR_SEMANTICO;
}

bool SemanticAnalysis::esTipo(const std::string& lexema) {
    // Un lexema es un tipo válido si la traducción anterior no produce error.
    return tipoDesdeLexema(lexema) != TipoDato::ERROR_SEMANTICO;
}

bool SemanticAnalysis::esOperador(const Token& token) {
    // Lista de gramemas que pueden participar en expresiones. La comparación
    // usa el token, no el texto, para distinguir operadores de identificadores.
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
    // En LIA el lexema ya contiene la representación del operador: +, *, <,
    // &&, etc. Se conserva en una función para centralizar esa conversión.
    return token.lexema;
}

std::string SemanticAnalysis::tablaSimbolos(
    const EstadoSemantico& estado
) {
    // Formatea la tabla de símbolos para salidas de consola o pruebas. La
    // ventana Qt construye su propia tabla visual desde el mismo estado.
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
    // Formatea el estado actual de las pilas. Recibe el estado por referencia
    // constante y las funciones auxiliares trabajan con copias, por lo que
    // imprimir no destruye las pilas reales.
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
    // Formatea el código intermedio generado como una tabla de texto.
    // El índice mostrado es CONT lógico (comienza en 1), mientras que el
    // vector interno usa índices de C++ que comienzan en 0.
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
