#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include "SemanticParser.h"
#include "Token.h"

#include <string>
#include <vector>

/**
 * Resultado que consume la ventana semantica.
 *
 * En la integracion actual, Parser llena `estado` mientras analiza. Por eso
 * `analizar()` ya no es el camino principal de ejecucion: se conserva como
 * herramienta auxiliar/legacy para escenarios que necesiten procesar una
 * secuencia de tokens de forma independiente.
 *
 * Los formateadores estaticos si son parte del camino actual y preparan la
 * representacion legible de pilas y cuadruplos que SemanticWindow presenta.
 */
struct SemanticAnalysisResult {
    Semantica::EstadoSemantico estado;
    std::vector<std::string> errores;
    bool correcto = false;
};

class SemanticAnalysis {
public:
    /**
     * Ejecuta el recorrido semantico independiente conservado por
     * compatibilidad. El flujo normal de la aplicacion usa las acciones
     * embebidas de Parser::Analiza() y no vuelve a llamar este metodo.
     */
    SemanticAnalysisResult analizar(const std::vector<Token>& tokens) const;

    /** Formatea la tabla de símbolos para la interfaz. */
    static std::string tablaSimbolos(
        const Semantica::EstadoSemantico& estado
    );

    /** Formatea las pilas semánticas para la interfaz. */
    static std::string pilas(
        const Semantica::EstadoSemantico& estado
    );

    /** Formatea los cuádruplos generados para la interfaz. */
    static std::string cuadruplos(
        const Semantica::EstadoSemantico& estado
    );

private:
    static Semantica::TipoDato tipoDesdeLexema(const std::string& lexema);
    static bool esTipo(const std::string& lexema);
    static bool esOperador(const Token& token);
    static std::string operador(const Token& token);

    static std::size_t procesarExpresion(
        const std::vector<Token>& tokens,
        std::size_t inicio,
        const std::vector<std::string>& terminadores,
        Semantica::EstadoSemantico& estado
    );
};

#endif // SEMANTIC_ANALYSIS_H
