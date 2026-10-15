#include "Parser.h"

#include <stdexcept>

// ============================================================================
// INTEGRACION SINTACTICO-SEMANTICA
//
// Parser.cpp conserva el driver predictivo original. La integracion no
// reemplaza la matriz ni la pila sintactica: agrega acciones en dos puntos:
//
// 1. Al aplicar una produccion, por ejemplo EXPR5' -> + EXPR5.
// 2. Al hacer match de un terminal, por ejemplo id, int o ';'.
//
// Esta separacion es importante porque algunas acciones dependen de la
// produccion elegida y otras necesitan el lexema del token actual.
// ============================================================================

/**
 * @file Parser.cpp
 * @brief Implementación del Analizador Sintáctico Predictivo de LIA
 *
 * DATOS (ya verificados contra el diseño en Excel):
 *  - MATRIZ_PREDICTIVA  29x45
 *  - PRODUCCIONES       67x9 (longitud + hasta 8 símbolos)
 *  - RESERVADA_A_CODIGO (17 palabras reservadas de la gramática)
 *
 * El driver (función Analiza) implementa fielmente el algoritmo de manejo
 * de la matriz predictiva visto en clase (los 4 casos terminal/no terminal).
 */

// ============================================================================
// MATRIZ PREDICTIVA M[X, a]   (29 no terminales x 45 terminales)
// Valor = número de producción (1-66) | -1 = error sintáctico
// ============================================================================
// Columnas:
//        0   id          9  float       18  loop        27  ;        36  <=
//        1   cteentera   10 char        19  break       28  ,        37  >
//        2   ctereal     11 bool        20  input       29  =        38  >=
//        3   ctenotacion 12 string      21  output      30  !        39  +
//        4   ctecaracter 13 if          22  (           31  ||       40  -
//        5   ctestring   14 else        23  )           32  &&       41  *
//        6   class       15 while       24  {           33  ==       42  /
//        7   def         16 do          25  }           34  !=       43  %
//        8   int         17 dowhile     26  :           35  <        44  $
// ============================================================================
const int Parser::MATRIZ_PREDICTIVA[NUM_NO_TERMINALES][NUM_TERMINALES] = {
//                id  cte ctr ctn ctc cts cls def int flt chr bol str  if els whl  do dwh lop brk inp out   (   )   {   }   :   ;   ,   =   !  ||  &&  ==  !=   <  <=   >  >=   +   -   *   /   %   $
/* 1 PROGRAM  */ { -1, -1, -1, -1, -1, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 2 DECLARA  */ {  3, -1, -1, -1, -1, -1, -1,  2, -1, -1, -1, -1, -1,  3, -1,  3,  3, -1,  3,  3,  3,  3, -1, -1, -1,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 3 LID      */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  5, -1,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 4 TIPO     */ { -1, -1, -1, -1, -1, -1, -1, -1,  6,  7,  8,  9, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 5 ESTATUTOS*/ { 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 19, 13, 14, 19, 17, 18, 16, 15, -1, -1, -1, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 6 EST_ASIG */ { 20, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 7 EST_IF   */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 8 SINO     */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 23, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 9 EST_WHILE*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*10 EST_DO   */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*11 EST_LOOP */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*12 EST_BREAK*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 27, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*13 EST_WRITE*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*14 LEXPR    */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 30, -1, -1, -1, -1, 29, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*15 EST_READ */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*16 LIDR     */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 33, -1, -1, -1, -1, 32, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*17 EXPR     */ { 34, 34, 34, 34, 34, 34, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 34, -1, -1, -1, -1, -1, -1, -1, 34, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*18 EXPR'    */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, -1, -1, -1, 36, 36, -1, -1, 35, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*19 EXPR2    */ { 37, 37, 37, 37, 37, 37, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 37, -1, -1, -1, -1, -1, -1, -1, 37, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*20 EXPR2'   */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 39, -1, -1, -1, 39, 39, -1, -1, 39, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*21 EXPR3    */ { 41, 41, 41, 41, 41, 41, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 41, -1, -1, -1, -1, -1, -1, -1, 40, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*22 EXPR4    */ { 42, 42, 42, 42, 42, 42, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 42, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*23 EXPR4'   */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 44, -1, -1, -1, 44, 44, -1, -1, 44, 44, 43, 43, 43, 43, 43, 43, -1, -1, -1, -1, -1, -1},
/*24 OPREL    */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 45, 46, 47, 48, 49, 50, -1, -1, -1, -1, -1, -1},
/*25 EXPR5    */ { 51, 51, 51, 51, 51, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*26 EXPR5'   */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 54, -1, -1, -1, 54, 54, -1, -1, 54, 54, 54, 54, 54, 54, 54, 54, 52, 53, -1, -1, -1, -1},
/*27 TERM     */ { 55, 55, 55, 55, 55, 55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/*28 TERM'    */ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 59, -1, -1, -1, 59, 59, -1, -1, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 56, 57, 58, -1},
/*29 FACT     */ { 60, 61, 62, 63, 64, 65, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

// ============================================================================
// MATRIZ DE PRODUCCIONES   (66 producciones x [longitud + 8 símbolos])
// Columna 0 = longitud del lado derecho (0 = épsilon)
// Columnas 1..8 = códigos de los símbolos EN ORDEN DE LECTURA
// El driver los inserta a la pila EN ORDEN INVERSO (del último al primero)
// ============================================================================
const int Parser::PRODUCCIONES[NUM_PRODUCCIONES][MAX_LADO_DERECHO + 1] = {
/* 1 PROGRAM -> class ( id ) { DECLARA ESTATUTOS } */ {8, 200,119,101,120,129,  2,  5,130},
/* 2 DECLARA -> def id LID : TIPO ; DECLARA        */ {7, 201,101,  3,131,  4,123,  2,  0},
/* 3 DECLARA -> epsilon                            */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/* 4 LID -> , id LID                               */ {3, 124,101,  3,  0,  0,  0,  0,  0},
/* 5 LID -> epsilon                                */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/* 6 TIPO -> int                                   */ {1, 202,  0,  0,  0,  0,  0,  0,  0},
/* 7 TIPO -> float                                 */ {1, 203,  0,  0,  0,  0,  0,  0,  0},
/* 8 TIPO -> char                                  */ {1, 204,  0,  0,  0,  0,  0,  0,  0},
/* 9 TIPO -> bool                                  */ {1, 205,  0,  0,  0,  0,  0,  0,  0},
/*10 TIPO -> string                                */ {1, 206,  0,  0,  0,  0,  0,  0,  0},
/*11 ESTATUTOS -> EST_ASIG ESTATUTOS               */ {2,   6,  5,  0,  0,  0,  0,  0,  0},
/*12 ESTATUTOS -> EST_IF ESTATUTOS                 */ {2,   7,  5,  0,  0,  0,  0,  0,  0},
/*13 ESTATUTOS -> EST_WHILE ESTATUTOS              */ {2,   9,  5,  0,  0,  0,  0,  0,  0},
/*14 ESTATUTOS -> EST_DO ESTATUTOS                 */ {2,  10,  5,  0,  0,  0,  0,  0,  0},
/*15 ESTATUTOS -> EST_READ ESTATUTOS               */ {2,  15,  5,  0,  0,  0,  0,  0,  0},
/*16 ESTATUTOS -> EST_WRITE ESTATUTOS              */ {2,  13,  5,  0,  0,  0,  0,  0,  0},
/*17 ESTATUTOS -> EST_LOOP ESTATUTOS               */ {2,  11,  5,  0,  0,  0,  0,  0,  0},
/*18 ESTATUTOS -> EST_BREAK ESTATUTOS              */ {2,  12,  5,  0,  0,  0,  0,  0,  0},
/*19 ESTATUTOS -> epsilon                          */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*20 EST_ASIG -> id = EXPR ;                       */ {4, 101,109, 17,123,  0,  0,  0,  0},
/*21 EST_IF -> if ( EXPR ) { ESTATUTOS SINO }      */ {8, 207,119, 17,120,129,  5,  8,130},
/*22 SINO -> else ESTATUTOS                        */ {2, 208,  5,  0,  0,  0,  0,  0,  0},
/*23 SINO -> epsilon                               */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*24 EST_WHILE -> while ( EXPR ) { ESTATUTOS }     */ {7, 209,119, 17,120,129,  5,130,  0},
/*25 EST_DO -> do ESTATUTOS dowhile ( EXPR ) ;     */ {7, 210,  5,211,119, 17,120,123,  0},
/*26 EST_LOOP -> loop { ESTATUTOS }                */ {4, 212,129,  5,130,  0,  0,  0,  0},
/*27 EST_BREAK -> break ;                          */ {2, 213,123,  0,  0,  0,  0,  0,  0},
/*28 EST_WRITE -> output ( EXPR LEXPR ) ;          */ {6, 215,119, 17, 14,120,123,  0,  0},
/*29 LEXPR -> , EXPR LEXPR                         */ {3, 124, 17, 14,  0,  0,  0,  0,  0},
/*30 LEXPR -> epsilon                              */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*31 EST_READ -> input ( id LIDR ) ;               */ {6, 214,119,101, 16,120,123,  0,  0},
/*32 LIDR -> , id LIDR                             */ {3, 124,101, 16,  0,  0,  0,  0,  0},
/*33 LIDR -> epsilon                               */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*34 EXPR -> EXPR2 EXPR'                           */ {2,  19, 18,  0,  0,  0,  0,  0,  0},
/*35 EXPR' -> || EXPR                              */ {2, 118, 17,  0,  0,  0,  0,  0,  0},
/*36 EXPR' -> epsilon                              */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*37 EXPR2 -> EXPR3 EXPR2'                         */ {2,  21, 20,  0,  0,  0,  0,  0,  0},
/*38 EXPR2' -> && EXPR2                            */ {2, 117, 19,  0,  0,  0,  0,  0,  0},
/*39 EXPR2' -> epsilon                             */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*40 EXPR3 -> ! EXPR4                              */ {2, 116, 22,  0,  0,  0,  0,  0,  0},
/*41 EXPR3 -> EXPR4                                */ {1,  22,  0,  0,  0,  0,  0,  0,  0},
/*42 EXPR4 -> EXPR5 EXPR4'                         */ {2,  25, 23,  0,  0,  0,  0,  0,  0},
/*43 EXPR4' -> OPREL EXPR5                         */ {2,  24, 25,  0,  0,  0,  0,  0,  0},
/*44 EXPR4' -> epsilon                             */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*45 OPREL -> ==                                   */ {1, 110,  0,  0,  0,  0,  0,  0,  0},
/*46 OPREL -> !=                                   */ {1, 115,  0,  0,  0,  0,  0,  0,  0},
/*47 OPREL -> <                                    */ {1, 111,  0,  0,  0,  0,  0,  0,  0},
/*48 OPREL -> <=                                   */ {1, 112,  0,  0,  0,  0,  0,  0,  0},
/*49 OPREL -> >                                    */ {1, 113,  0,  0,  0,  0,  0,  0,  0},
/*50 OPREL -> >=                                   */ {1, 114,  0,  0,  0,  0,  0,  0,  0},
/*51 EXPR5 -> TERM EXPR5'                          */ {2,  27, 26,  0,  0,  0,  0,  0,  0},
/*52 EXPR5' -> + EXPR5                             */ {2, 105, 25,  0,  0,  0,  0,  0,  0},
/*53 EXPR5' -> - EXPR5                             */ {2, 106, 25,  0,  0,  0,  0,  0,  0},
/*54 EXPR5' -> epsilon                             */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*55 TERM -> FACT TERM'                            */ {2,  29, 28,  0,  0,  0,  0,  0,  0},
/*56 TERM' -> * TERM                               */ {2, 107, 27,  0,  0,  0,  0,  0,  0},
/*57 TERM' -> / TERM                               */ {2, 108, 27,  0,  0,  0,  0,  0,  0},
/*58 TERM' -> % TERM                               */ {2, 128, 27,  0,  0,  0,  0,  0,  0},
/*59 TERM' -> epsilon                              */ {0,  -1,  0,  0,  0,  0,  0,  0,  0},
/*60 FACT -> id                                    */ {1, 101,  0,  0,  0,  0,  0,  0,  0},
/*61 FACT -> cteentera                             */ {1, 102,  0,  0,  0,  0,  0,  0,  0},
/*62 FACT -> ctereal                               */ {1, 103,  0,  0,  0,  0,  0,  0,  0},
/*63 FACT -> ctenotacion                           */ {1, 104,  0,  0,  0,  0,  0,  0,  0},
/*64 FACT -> ctecaracter                           */ {1, 125,  0,  0,  0,  0,  0,  0,  0},
/*65 FACT -> ctestring                             */ {1, 126,  0,  0,  0,  0,  0,  0,  0},
/*66 FACT -> ( EXPR )                              */ {3, 119, 17,120,  0,  0,  0,  0,  0},
// La constante se limita inicialmente a un FACT literal. La forma:
//     const PI : real = 3.1416;
// se registra cuando se hace match con ';'.
/*67 DECLARA -> const id : TIPO = FACT ; DECLARA   */ {8, 216,101,131, 4,109, 29,123,  2}
};

// ============================================================================
// TABLA DE PALABRAS RESERVADAS DEL PARSER (lexema -> código serie 200)
// ============================================================================
const std::map<std::string, int> Parser::RESERVADA_A_CODIGO = {
    {"class",   PR_CLASS},
    {"def",     PR_DEF},
    {"int",     PR_INT},
    {"float",   PR_FLOAT},
    // `real` es un alias de `float`: ambos representan TipoDato::REAL y
    // reutilizan la misma columna y producción de la gramática.
    {"real",    PR_FLOAT},
    {"char",    PR_CHAR},
    {"bool",    PR_BOOL},
    {"string",  PR_STRING},
    {"if",      PR_IF},
    {"else",    PR_ELSE},
    {"while",   PR_WHILE},
    {"do",      PR_DO},
    {"dowhile", PR_DOWHILE},
    {"loop",    PR_LOOP},
    {"break",   PR_BREAK},
    {"input",   PR_INPUT},
    {"output",  PR_OUTPUT},
    // const no se confunde con un identificador porque el lexer la entrega
    // como palabra reservada y codigoDelToken la traduce a PR_CONST.
    {"const",   PR_CONST}
    // Nota: "endclass" es reservada del lexer pero NO forma parte de la
    // gramática (el programa cierra con '}'), por eso no aparece aquí.
};

// ========== CONSTRUCTOR Y DESTRUCTOR ==========

Parser::Parser()
    : lexer(nullptr),
      sintaxisCorrecta(false),
      semanticaCorrecta(true),
      declaracionActiva(false),
      constanteActiva(false),
      asignacionActiva(false),
      entradaPorIdentificadorActiva(false),
      parentesisDeExpresionAbiertos(0) {
    // Constructor por defecto
}

Parser::~Parser() {
    // Destructor (no hay recursos dinámicos que liberar)
}

// ========== FUNCIÓN PRINCIPAL DE ANÁLISIS (EL DRIVER) ==========

bool Parser::Analiza() {
    // Reiniciar estado
    reset();

    // Sin léxico conectado no se puede analizar (defensivo)
    if (lexer == nullptr) {
        return false;
    }

    // 1. INICIALIZAR LA PILA: $ en el fondo, PROGRAM en el tope
    pila.push(FIN_ENTRADA);
    pila.push(NT_PROGRAM);

    // Pedir el PRIMER token al léxico (modelo bajo demanda "Dame_un_token")
    tokenActual = lexer->Dame_un_token();
    if (!validarTokenLexico()) {
        return false;   // Error léxico detectado y reportado
    }

    // 2. BUCLE PRINCIPAL DEL DRIVER
    // Cada paso de la traza se registra ANTES de modificar la pila, para que
    // cada renglón muestre: pila actual | token actual | acción que se ejecuta
    // (mismo formato que la "Pila de Ejecución" vista en clase)
    // Las excepciones de la capa semantica se capturan al nivel del driver.
    // Asi un error de tipos no se confunde con un error de sintaxis.
    try {
    while (!pila.empty()) {
        int X = pila.top();             // símbolo del tope de la pila
        int a = codigoTokenActual();    // código del token actual (o $)

        if (X >= 100) {
            // ================= X ES TERMINAL =================
            if (X == a && a == FIN_ENTRADA) {
                // CASO 1: tope == token == $  -> ACEPTAR
                registrarPaso("$ = $. La cadena de entrada es SINTACTICAMENTE CORRECTA");
                sintaxisCorrecta = true;
                pila.pop();
                break;

            } else if (X == a) {
                // CASO 2: MATCH -> sacar de la pila y PEDIR EL SIGUIENTE TOKEN
                // al léxico (esto es el "Dame_un_token()" / "Va_token" de clase)
                registrarPaso(nombreSimbolo(X) + " = '" + lexemaActual() +
                              "'. Match: sacar de la pila y pedir nuevo token");
                // El terminal se reconoce correctamente. Antes de pedir el
                // siguiente token se ejecuta cualquier accion semantica que
                // dependa de este match.
                try {
                    ejecutarAccionDeMatch(X);
                } catch (const std::exception& error) {
                    // La sintaxis puede continuar aunque una accion
                    // semantica falle; ambas fases deben reportarse por
                    // separado al usuario.
                    semanticaCorrecta = false;
                    erroresSemanticos.push_back(error.what());
                    registrarPaso(
                        "ERROR SEMANTICO: " + std::string(error.what())
                    );
                }
                pila.pop();
                tokenActual = lexer->Dame_un_token();
                if (!validarTokenLexico()) {
                    return false;   // Error léxico detectado y reportado
                }

            } else {
                // CASO 3: el terminal esperado NO coincide con el token
                if (a == FIN_ENTRADA) {
                    // La entrada se agotó pero la pila esperaba más símbolos
                    registrarPaso("ERROR: fin de archivo, se esperaba '" +
                                  nombreSimbolo(X) + "'");
                    reportarError(ERROR_SIN_FIN_INESPERADO,
                                  nombreSimbolo(X), "$ (fin de archivo)");
                } else {
                    registrarPaso("ERROR: se esperaba '" + nombreSimbolo(X) +
                                  "' y se encontro '" + lexemaActual() + "'");
                    reportarError(ERROR_SIN_TERMINAL_NO_COINCIDE,
                                  nombreSimbolo(X), lexemaActual());
                }
                break;  // detener el análisis en el primer error
            }

        } else {
            // ================= X ES NO TERMINAL =================
            int col = columnaDe(a);
            int n = (col != -1) ? MATRIZ_PREDICTIVA[X - 1][col] : -1;
            // La tabla original se conserva con sus 45 columnas. La nueva
            // palabra reservada const se dirige explícitamente a la
            // producción 67 para no desplazar las columnas existentes ni
            // invalidar las producciones que ya estaban probadas.
            if (X == NT_DECLARA && a == PR_CONST) {
                n = 67;
            }
            // La gramática histórica tenía input/output intercambiados.
            // Estas dos rutas conservan la matriz original sin desplazar
            // columnas y seleccionan ahora los estatutos convencionales:
            // output(EXPR, ...) e input(id, ...).
            if (X == NT_EST_WRITE && a == PR_INPUT) {
                n = 31;
            }
            if (X == NT_EST_READ && a == PR_OUTPUT) {
                n = 28;
            }

            if (n != -1) {
                // Aplicar la producción n: sacar X e insertar su lado
                // derecho EN ORDEN INVERSO (el primer símbolo queda al tope)
                int longitud = PRODUCCIONES[n - 1][0];

                // Las acciones semánticas se ejecutan al reconocer la
                // producción, antes de insertar sus símbolos. Esto permite
                // colocar un operador en POper antes de analizar el operando
                // que aparece a la derecha en la gramática LL(1).
                // La tabla predictiva eligio una produccion valida. La
                // accion se ejecuta antes de insertar el lado derecho para
                // conservar el orden conceptual del diagrama semantico.
                try {
                    ejecutarAccionDeProduccion(n);
                } catch (const std::exception& error) {
                    semanticaCorrecta = false;
                    erroresSemanticos.push_back(error.what());
                    registrarPaso(
                        "ERROR SEMANTICO: " + std::string(error.what())
                    );
                }

                if (longitud == 0) {
                    // Producción épsilon: solo se elimina X de la pila
                    registrarPaso("M[" + nombreSimbolo(X) + ", " + nombreSimbolo(a) +
                                  "] = " + std::to_string(n) +
                                  ". Deriva a epsilon: solo se elimina " + nombreSimbolo(X));
                    pila.pop();
                } else {
                    registrarPaso("M[" + nombreSimbolo(X) + ", " + nombreSimbolo(a) +
                                  "] = " + std::to_string(n) + ". Sacar " + nombreSimbolo(X) +
                                  " e insertar su derivacion (invertida)");
                    pila.pop();
                    for (int i = longitud; i >= 1; i--) {
                        pila.push(PRODUCCIONES[n - 1][i]);
                    }
                }

            } else {
                // Celda vacía (-1): el token no puede iniciar la construcción X
                registrarPaso("ERROR: M[" + nombreSimbolo(X) + ", " + nombreSimbolo(a) +
                              "] = -1. Token inesperado '" + lexemaActual() + "'");
                reportarError(ERROR_SIN_TOKEN_INESPERADO,
                              esperadosDe(X), lexemaActual());
                break;  // detener el análisis en el primer error
            }
        }
    }
    } catch (const std::exception& error) {
            // Un error semántico no debe convertirse artificialmente en un error
            // sintáctico. Se conserva la traza y se informa en su propia fase.
            semanticaCorrecta = false;
            erroresSemanticos.push_back(error.what());
            registrarPaso("ERROR SEMANTICO: " + std::string(error.what()));
    }
    // Nota: ya no se necesita el chequeo de "tokens sobrantes": si tras el
    // programa quedan tokens, el tope será $ y el token actual será otro,
    // cayendo de forma natural en el CASO 3 (error de terminal).

    return sintaxisCorrecta;
}

// ========== FUNCIONES DE CONTROL ==========

void Parser::setLexer(Lexer* lex) {
    lexer = lex;
}

void Parser::reset() {
    sintaxisCorrecta = false;
    tokenActual = Token();   // token vacío hasta pedir el primero
    errores.clear();
    traza.clear();
    Semantica::reiniciarEstado(estadoSemantico);
    erroresSemanticos.clear();
    semanticaCorrecta = true;
    declaracionActiva = false;
    constanteActiva = false;
    asignacionActiva = false;
    entradaPorIdentificadorActiva = false;
    parentesisDeExpresionAbiertos = 0;
    idConstantePendiente.clear();
    tipoConstantePendiente = Semantica::TipoDato::ERROR_SEMANTICO;
    valorConstantePendiente.clear();
    // Vaciar la pila (std::stack no tiene clear())
    while (!pila.empty()) {
        pila.pop();
    }
}

// ========== FUNCIONES DE ACCESO A RESULTADOS ==========

const std::vector<ErrorSintactico>& Parser::getErrores() const {
    return errores;
}

const std::vector<std::string>& Parser::getTraza() const {
    return traza;
}

bool Parser::esCorrecto() const {
    return sintaxisCorrecta;
}

bool Parser::semanticaEsCorrecta() const {
    return semanticaCorrecta;
}

const Semantica::EstadoSemantico& Parser::getEstadoSemantico() const {
    return estadoSemantico;
}

const std::vector<std::string>& Parser::getErroresSemanticos() const {
    return erroresSemanticos;
}

// ============================================================================
// ACCIONES SEMANTICAS EMBEBIDAS EN EL DRIVER LL(1)
//
// La tabla sintactica sigue tomando todas las decisiones. Estas funciones
// solamente agregan el efecto semantico que corresponde a cada produccion:
// apilar operandos, guardar operadores, reducir expresiones y cerrar
// declaraciones/asignaciones.
// ============================================================================

/**
 * @brief Ejecuta las acciones semanticas colocadas alrededor de producciones.
 *
 * Las producciones epsilon son especialmente importantes: en la gramatica
 * LL(1) representan el momento en que termina un nivel de expresion. Por eso
 * las producciones 36, 39, 44, 54 y 59 disparan reducciones de POper.
 */
void Parser::ejecutarAccionDeProduccion(int numeroProduccion) {
    using namespace Semantica;

    switch (numeroProduccion) {
        case 2: // DECLARA -> def id LID : TIPO ;
            // Se limpia la lista temporal antes de acumular los ids de esta
            // declaracion. El tipo todavia no se conoce en este punto.
            Declaraciones::iniciarDeclaracion(estadoSemantico);
            declaracionActiva = true;
            break;

        case 67: // DECLARA -> const id : TIPO = FACT ; DECLARA
            // Se abre un contexto independiente. A diferencia de `def`, no
            // se acumula una lista de ids: esta sintaxis declara una sola
            // constante y exige que tenga un valor inicial.
            constanteActiva = true;
            declaracionActiva = false;
            idConstantePendiente.clear();
            tipoConstantePendiente =
                TipoDato::ERROR_SEMANTICO;
            valorConstantePendiente.clear();
            break;

        case 20: // EST_ASIG -> id = EXPR ;
            // El token actual es el id destino porque la produccion se
            // selecciona antes de hacer match con dicho terminal.
            VerificacionSemantica::iniciarAsignacion(
                estadoSemantico,
                tokenActual.lexema
            );
            asignacionActiva = true;
            break;

        case 28: // output ( EXPR LEXPR ) ;
            // Cada expresion se convierte en un cuadruplo de salida al
            // terminar la expresion.
            break;

        case 29: // LEXPR -> , EXPR LEXPR
            // La expresion anterior termina justo antes de la coma.
            if (!estadoSemantico.PilaO.empty()) {
                const std::string resultado =
                    Expresiones::obtenerResultado(estadoSemantico);
                EntradaSalida::generarSalida(
                    estadoSemantico,
                    resultado
                );
            }
            break;

        case 30: // LEXPR -> epsilon
            // Cierra la ultima expresion de output antes de ')'.
            if (!estadoSemantico.PilaO.empty()) {
                const std::string resultado =
                    Expresiones::obtenerResultado(estadoSemantico);
                EntradaSalida::generarSalida(
                    estadoSemantico,
                    resultado
                );
            }
            break;

        case 31: // input ( id LIDR ) ;
            // Cada identificador de la lista se convierte en un cuadruplo
            // input y se marca como valor desconocido.
            entradaPorIdentificadorActiva = true;
            break;

        case 35: // EXPR' -> || EXPR
            // OR tiene la precedencia logica mas baja.
            Expresiones::recibirOperador(estadoSemantico, "||");
            break;
        case 38: // EXPR2' -> && EXPR2
            Expresiones::recibirOperador(estadoSemantico, "&&");
            break;
        case 40: // EXPR3 -> ! EXPR4
            // '!' se guarda antes de analizar el operando que le sigue.
            Expresiones::recibirOperadorUnario(estadoSemantico, "!");
            break;
        case 43: // EXPR4' -> OPREL EXPR5
            Expresiones::recibirOperador(
                estadoSemantico,
                tokenActual.lexema
            );
            break;
        case 52: // EXPR5' -> + EXPR5
            Expresiones::recibirOperador(estadoSemantico, "+");
            break;
        case 53: // EXPR5' -> - EXPR5
            Expresiones::recibirOperador(estadoSemantico, "-");
            break;
        case 56: // TERM' -> * TERM
            Expresiones::recibirOperador(estadoSemantico, "*");
            break;
        case 57: // TERM' -> / TERM
            Expresiones::recibirOperador(estadoSemantico, "/");
            break;
        case 58: // TERM' -> % TERM
            Expresiones::recibirOperador(estadoSemantico, "%");
            break;

        case 60: // FACT -> id
            // El identificador se consulta en la tabla de simbolos y se
            // apilan su representacion de memoria y su tipo.
            Expresiones::apilarIdentificador(
                estadoSemantico,
                tokenActual.lexema
            );
            break;
        case 61: // FACT -> cteentera
            if (constanteActiva) {
                // Durante una constante el literal no va a PilaO: se guarda
                // como valor pendiente para declararConstante().
                valorConstantePendiente = tokenActual.lexema;
                break;
            }
            Expresiones::apilarConstante(
                estadoSemantico,
                tokenActual.lexema,
                TipoDato::ENTERO
            );
            break;
        case 62: // FACT -> ctereal
        case 63: // FACT -> ctenotacion
            if (constanteActiva) {
                // Las dos formas representan un valor REAL: decimal o
                // notacion cientifica.
                valorConstantePendiente = tokenActual.lexema;
                break;
            }
            Expresiones::apilarConstante(
                estadoSemantico,
                tokenActual.lexema,
                TipoDato::REAL
            );
            break;
        case 64: // FACT -> ctecaracter
            if (constanteActiva) {
                // Se conserva el lexema completo, incluidas las comillas,
                // para que la validacion semantica pueda revisarlo.
                valorConstantePendiente = tokenActual.lexema;
                break;
            }
            Expresiones::apilarConstante(
                estadoSemantico,
                tokenActual.lexema,
                TipoDato::CARACTER
            );
            break;
        case 65: // FACT -> ctestring
            if (constanteActiva) {
                // La cadena tambien se conserva textual para la tabla visual.
                valorConstantePendiente = tokenActual.lexema;
                break;
            }
            Expresiones::apilarConstante(
                estadoSemantico,
                tokenActual.lexema,
                TipoDato::CADENA
            );
            break;
        case 66: // FACT -> ( EXPR )
            // La marca '(' evita reducir operadores de fuera del parentesis.
            Expresiones::abrirParentesis(estadoSemantico);
            ++parentesisDeExpresionAbiertos;
            break;

        case 36: // EXPR' -> epsilon
            // Al terminar EXPR ya no quedan operadores OR pendientes.
            reducirOperadoresConPrecedenciaMinima(1);
            break;
        case 39: // EXPR2' -> epsilon
            reducirOperadoresConPrecedenciaMinima(2);
            break;
        case 44: // EXPR4' -> epsilon
            reducirOperadoresConPrecedenciaMinima(3);
            break;
        case 54: // EXPR5' -> epsilon
            reducirOperadoresConPrecedenciaMinima(4);
            break;
        case 59: // TERM' -> epsilon
            reducirOperadoresConPrecedenciaMinima(5);
            break;
        default:
            // Las demas producciones son puramente sintacticas por ahora.
            break;
    }
}

/**
 * @brief Ejecuta acciones que necesitan el token que acaba de hacer match.
 *
 * A diferencia de ejecutarAccionDeProduccion(), aqui tokenActual contiene
 * directamente el lexema reconocido por el lexer. Esto permite registrar
 * nombres de variables, tipos y destinos de entrada/salida.
 */
void Parser::ejecutarAccionDeMatch(int terminalEsperado) {
    using namespace Semantica;

    // El identificador de una declaracion de variable se acumula antes de
    // conocer el tipo. Para una constante se conserva por separado porque su
    // produccion no utiliza LID.
    if (constanteActiva && terminalEsperado == IDENTIFICADOR) {
        idConstantePendiente = tokenActual.lexema;
    } else if (declaracionActiva && terminalEsperado == IDENTIFICADOR) {
        Declaraciones::acumularIdentificador(
            estadoSemantico,
            tokenActual.lexema
        );
    }

    // El tipo cierra semanticamente la declaracion de variables. Todos los
    // ids acumulados reciben este mismo tipo y una direccion logica distinta.
    if (declaracionActiva &&
        (terminalEsperado == PR_INT ||
         terminalEsperado == PR_FLOAT ||
         terminalEsperado == PR_CHAR ||
         terminalEsperado == PR_BOOL ||
         terminalEsperado == PR_STRING)) {
        TipoDato tipo = TipoDato::ERROR_SEMANTICO;
        if (terminalEsperado == PR_INT) tipo = TipoDato::ENTERO;
        if (terminalEsperado == PR_FLOAT) tipo = TipoDato::REAL;
        if (terminalEsperado == PR_CHAR) tipo = TipoDato::CARACTER;
        if (terminalEsperado == PR_BOOL) tipo = TipoDato::BOOLEANO;
        if (terminalEsperado == PR_STRING) tipo = TipoDato::CADENA;
        Declaraciones::declararVariables(estadoSemantico, tipo);
    } else if (constanteActiva &&
               (terminalEsperado == PR_INT ||
                terminalEsperado == PR_FLOAT ||
                terminalEsperado == PR_CHAR ||
                terminalEsperado == PR_BOOL ||
                terminalEsperado == PR_STRING)) {
        // En una constante el tipo se guarda, pero el registro se retrasa
        // hasta ';' para que tambien exista el literal inicial.
        if (terminalEsperado == PR_INT) tipoConstantePendiente = TipoDato::ENTERO;
        if (terminalEsperado == PR_FLOAT) tipoConstantePendiente = TipoDato::REAL;
        if (terminalEsperado == PR_CHAR) tipoConstantePendiente = TipoDato::CARACTER;
        if (terminalEsperado == PR_BOOL) tipoConstantePendiente = TipoDato::BOOLEANO;
        if (terminalEsperado == PR_STRING) tipoConstantePendiente = TipoDato::CADENA;
    }

    // EST_READ representa input(id, ...). Cada identificador reconocido
    // genera su cuadruplo de entrada.
    if (entradaPorIdentificadorActiva &&
        terminalEsperado == IDENTIFICADOR) {
        EntradaSalida::generarEntrada(
            estadoSemantico,
            tokenActual.lexema
        );
    }

    // Cierre de un parentesis que fue abierto por FACT -> ( EXPR ).
    if (terminalEsperado == PAREN_CIERRA &&
        parentesisDeExpresionAbiertos > 0) {
        Expresiones::cerrarParentesis(estadoSemantico);
        --parentesisDeExpresionAbiertos;
    }

    // '=' no entra en POper. Solo termina el contexto de una asignacion.
    // POper debe contener exclusivamente operadores de expresiones.
    if (asignacionActiva && terminalEsperado == PUNTO_COMA) {
        Expresiones::finalizarAsignacion(estadoSemantico);
        asignacionActiva = false;
    }

    if (declaracionActiva && terminalEsperado == PUNTO_COMA) {
        declaracionActiva = false;
    }
    if (constanteActiva && terminalEsperado == PUNTO_COMA) {
        // Este es el punto de commit de la declaracion: todos los datos ya
        // fueron reconocidos y declararConstante valida tipo, redeclaracion
        // y valor antes de insertar el simbolo.
        Declaraciones::declararConstante(
            estadoSemantico,
            idConstantePendiente,
            tipoConstantePendiente,
            valorConstantePendiente
        );
        constanteActiva = false;
        idConstantePendiente.clear();
        valorConstantePendiente.clear();
        tipoConstantePendiente = TipoDato::ERROR_SEMANTICO;
    }

    if (entradaPorIdentificadorActiva && terminalEsperado == PUNTO_COMA) {
        entradaPorIdentificadorActiva = false;
    }
}

/**
 * @brief Implementa las acciones de reduccion de expresiones.
 *
 * La pila POper conserva operadores mientras se reconocen operandos. Cuando
 * la gramatica llega a una produccion epsilon de un nivel, esta funcion saca
 * los operadores que ya deben evaluarse, consulta el cubo semantico y crea
 * los cuádruplos correspondientes.
 */
void Parser::reducirOperadoresConPrecedenciaMinima(
    int precedenciaMinima
) {
    using namespace Semantica;

    while (!estadoSemantico.POper.empty() &&
           estadoSemantico.POper.top() != "(" &&
           GeneracionCodigo::precedencia(
               estadoSemantico.POper.top()
           ) >= precedenciaMinima) {
        const std::string operador = estadoSemantico.POper.top();
        estadoSemantico.POper.pop();

        if (operador == "!" || operador == "NEG") {
            GeneracionCodigo::reducirOperacionUnaria(
                estadoSemantico,
                operador
            );
        } else {
            GeneracionCodigo::reducirOperacion(
                estadoSemantico,
                operador
            );
        }
    }
}

// ========== FUNCIONES AUXILIARES (PUENTE LEXER-PARSER) ==========

int Parser::codigoDelToken(const Token& t) const {
    // Las palabras reservadas llegan del lexer con gramema 100:
    // se distinguen por su lexema usando la tabla serie 200
    if (t.gramema == 100) {
        std::map<std::string, int>::const_iterator it = RESERVADA_A_CODIGO.find(t.lexema);
        if (it != RESERVADA_A_CODIGO.end()) {
            return it->second;
        }
        // Reservada que no usa la gramática (ej: endclass):
        // se regresa el gramema original y caerá en celda -1 (error sintáctico)
        return t.gramema;
    }
    // Para el resto de tokens el gramema del lexer ES el código del terminal
    return t.gramema;
}

int Parser::columnaDe(int codigoTerminal) const {
    switch (codigoTerminal) {
        // --- identificador y constantes ---
        case 101: return 0;   // id
        case 102: return 1;   // cteentera
        case 103: return 2;   // ctereal
        case 104: return 3;   // ctenotacion
        case 125: return 4;   // ctecaracter
        case 126: return 5;   // ctestring
        // --- palabras reservadas (serie 200) ---
        case PR_CLASS:   return 6;
        case PR_DEF:     return 7;
        case PR_INT:     return 8;
        case PR_FLOAT:   return 9;
        case PR_CHAR:    return 10;
        case PR_BOOL:    return 11;
        case PR_STRING:  return 12;
        case PR_IF:      return 13;
        case PR_ELSE:    return 14;
        case PR_WHILE:   return 15;
        case PR_DO:      return 16;
        case PR_DOWHILE: return 17;
        case PR_LOOP:    return 18;
        case PR_BREAK:   return 19;
        case PR_INPUT:   return 20;
        case PR_OUTPUT:  return 21;
        // --- delimitadores ---
        case 119: return 22;  // (
        case 120: return 23;  // )
        case 129: return 24;  // {
        case 130: return 25;  // }
        case 131: return 26;  // :
        case 123: return 27;  // ;
        case 124: return 28;  // ,
        // --- operadores ---
        case 109: return 29;  // =  (asignación)
        case 116: return 30;  // !
        case 118: return 31;  // ||
        case 117: return 32;  // &&
        case 110: return 33;  // ==
        case 115: return 34;  // !=
        case 111: return 35;  // <
        case 112: return 36;  // <=
        case 113: return 37;  // >
        case 114: return 38;  // >=
        case 105: return 39;  // +
        case 106: return 40;  // -
        case 107: return 41;  // *
        case 108: return 42;  // /
        case 128: return 43;  // %
        // --- fin de entrada ---
        case FIN_ENTRADA: return 44;  // $

        default:  return -1;  // código que no es terminal de la gramática
    }
}

int Parser::codigoTokenActual() const {
    // El token actual ya viene del léxico (incluido el token "$" al final).
    return codigoDelToken(tokenActual);
}

bool Parser::validarTokenLexico() {
    // Verifica si el token actual es un error léxico.
   
    if (tokenActual.gramema >= 500 && tokenActual.gramema < 600) {
        // Es un error léxico: reportarlo y detener
        reportarError(tokenActual.gramema, "token válido", tokenActual.lexema);
        registrarPaso("ERROR LEXICO (" + std::to_string(tokenActual.gramema) +
                      "): '" + tokenActual.lexema + "'. Se detiene el análisis");
        return false;
    }
    return true;
}

std::string Parser::lexemaActual() const {
    return tokenActual.lexema;
}

int Parser::lineaActual() const {
    return tokenActual.linea;
}

int Parser::columnaActual() const {
    return tokenActual.columna;
}

std::string Parser::nombreSimbolo(int codigo) const {
    switch (codigo) {
        // --- no terminales (1-29) ---
        case NT_PROGRAM:   return "PROGRAM";
        case NT_DECLARA:   return "DECLARA";
        case NT_LID:       return "LID";
        case NT_TIPO:      return "TIPO";
        case NT_ESTATUTOS: return "ESTATUTOS";
        case NT_EST_ASIG:  return "EST_ASIG";
        case NT_EST_IF:    return "EST_IF";
        case NT_SINO:      return "SINO";
        case NT_EST_WHILE: return "EST_WHILE";
        case NT_EST_DO:    return "EST_DO";
        case NT_EST_LOOP:  return "EST_LOOP";
        case NT_EST_BREAK: return "EST_BREAK";
        case NT_EST_WRITE: return "EST_WRITE";
        case NT_LEXPR:     return "LEXPR";
        case NT_EST_READ:  return "EST_READ";
        case NT_LIDR:      return "LIDR";
        case NT_EXPR:      return "EXPR";
        case NT_EXPR_P:    return "EXPR'";
        case NT_EXPR2:     return "EXPR2";
        case NT_EXPR2_P:   return "EXPR2'";
        case NT_EXPR3:     return "EXPR3";
        case NT_EXPR4:     return "EXPR4";
        case NT_EXPR4_P:   return "EXPR4'";
        case NT_OPREL:     return "OPREL";
        case NT_EXPR5:     return "EXPR5";
        case NT_EXPR5_P:   return "EXPR5'";
        case NT_TERM:      return "TERM";
        case NT_TERM_P:    return "TERM'";
        case NT_FACT:      return "FACT";
        // --- terminales: id y constantes ---
        case 101: return "id";
        case 102: return "cteentera";
        case 103: return "ctereal";
        case 104: return "ctenotacion";
        case 125: return "ctecaracter";
        case 126: return "ctestring";
        // --- terminales: palabras reservadas ---
        case PR_CLASS:   return "class";
        case PR_DEF:     return "def";
        case PR_INT:     return "int";
        case PR_FLOAT:   return "float";
        case PR_CHAR:    return "char";
        case PR_BOOL:    return "bool";
        case PR_STRING:  return "string";
        case PR_IF:      return "if";
        case PR_ELSE:    return "else";
        case PR_WHILE:   return "while";
        case PR_DO:      return "do";
        case PR_DOWHILE: return "dowhile";
        case PR_LOOP:    return "loop";
        case PR_BREAK:   return "break";
        case PR_INPUT:   return "input";
        case PR_OUTPUT:  return "output";
        case PR_CONST:   return "const";
        // --- terminales: delimitadores y operadores ---
        case 119: return "(";
        case 120: return ")";
        case 129: return "{";
        case 130: return "}";
        case 131: return ":";
        case 123: return ";";
        case 124: return ",";
        case 109: return "=";
        case 116: return "!";
        case 118: return "||";
        case 117: return "&&";
        case 110: return "==";
        case 115: return "!=";
        case 111: return "<";
        case 112: return "<=";
        case 113: return ">";
        case 114: return ">=";
        case 105: return "+";
        case 106: return "-";
        case 107: return "*";
        case 108: return "/";
        case 128: return "%";
        case FIN_ENTRADA: return "$";

        default: return "?(" + std::to_string(codigo) + ")";
    }
}

std::string Parser::esperadosDe(int codigoNoTerminal) const {
    // Los terminales esperados son las columnas con celda != -1 en la fila
    // del no terminal: salen DIRECTO de la matriz predictiva (sus FIRST,
    // más los FOLLOW si tiene producción épsilon)
    std::string resultado;
    if (codigoNoTerminal < 1 || codigoNoTerminal > NUM_NO_TERMINALES) {
        return resultado;
    }

    // Tabla inversa columna -> código de terminal (mismo orden que columnaDe)
    static const int CODIGO_POR_COLUMNA[NUM_TERMINALES] = {
        101, 102, 103, 104, 125, 126,                         // 0-5
        PR_CLASS, PR_DEF, PR_INT, PR_FLOAT, PR_CHAR,          // 6-10
        PR_BOOL, PR_STRING, PR_IF, PR_ELSE, PR_WHILE,         // 11-15
        PR_DO, PR_DOWHILE, PR_LOOP, PR_BREAK, PR_INPUT,       // 16-20
        PR_OUTPUT, 119, 120, 129, 130, 131, 123, 124,         // 21-28
        109, 116, 118, 117, 110, 115, 111, 112, 113, 114,     // 29-38
        105, 106, 107, 108, 128, FIN_ENTRADA                  // 39-44
    };

    for (int col = 0; col < NUM_TERMINALES; col++) {
        if (MATRIZ_PREDICTIVA[codigoNoTerminal - 1][col] != -1) {
            if (!resultado.empty()) {
                resultado += ", ";
            }
            resultado += nombreSimbolo(CODIGO_POR_COLUMNA[col]);
        }
    }
    return resultado;
}

void Parser::reportarError(int codigo, const std::string& esperado, const std::string& encontrado) {
    ErrorSintactico error(lineaActual(), columnaActual(), codigo, esperado, encontrado);
    errores.push_back(error);
}

void Parser::registrarPaso(const std::string& accion) {
    // Formato de la traza (estilo "Pila de Ejecución" vista en clase):
    //   PILA: <contenido> | ENTRADA: <lexema actual> | ACCION: <accion>
    std::string paso = "PILA: " + pilaToString() +
                       " | ENTRADA: " + lexemaActual() +
                       " | " + accion;
    traza.push_back(paso);
}

std::string Parser::pilaToString() const {
    // std::stack no permite iterar: se copia y se vacía la copia.
    // El tope queda a la IZQUIERDA del string resultante.
    std::stack<int> copia = pila;
    std::string resultado;
    while (!copia.empty()) {
        if (!resultado.empty()) {
            resultado += " ";
        }
        resultado += nombreSimbolo(copia.top());
        copia.pop();
    }
    return resultado;
}
