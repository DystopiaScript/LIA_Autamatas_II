#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include "SemanticParser.h"
#include "Token.h"

#include <string>
#include <vector>

/**
 * Resultado de la primera integración semántica.
 *
 * SemanticParser contiene las acciones individuales. Esta clase las coordina
 * sobre la secuencia de tokens ya aceptada por Parser y prepara el texto que
 * la ventana semántica mostrará al usuario.
 */
struct SemanticAnalysisResult {
    Semantica::EstadoSemantico estado;
    std::vector<std::string> errores;
    bool correcto = false;
};

class SemanticAnalysis {
public:
    /**
     * Ejecuta la fase semántica sobre tokens de un programa sintácticamente
     * correcto. En esta primera etapa procesa declaraciones, asignaciones,
     * expresiones, input y output.
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
