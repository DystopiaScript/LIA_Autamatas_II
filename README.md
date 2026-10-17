# Analizador LIA: lexer, parser y semántica

Proyecto académico de la materia **Lenguajes y Autómatas II (Autómatas II)**,
desarrollado en C++ con
Qt Widgets y CMake. La aplicación permite escribir o abrir un programa
`.lia`, visualizar sus tokens, ejecutar el parser predictivo LL(1) y revisar
el análisis semántico junto con el código intermedio generado.

## Flujo general

```text
Código LIA
   ↓
Lexer
   ↓
Tokens
   ↓
Parser predictivo LL(1)
   ↓
Acciones semánticas embebidas
   ↓
Tabla de símbolos + pilas + cuádruplos
   ↓
Ventana de resultados semánticos
```

El flujo principal ejecuta las acciones semánticas mientras el parser aplica
las producciones. `SemanticAnalysis.cpp` conserva además un analizador
auxiliar para pruebas y formateo textual; no sustituye el flujo principal del
parser.

## Funcionalidades actuales

- Analizador léxico con tokens, lexemas, líneas y columnas.
- Parser predictivo LL(1) con matriz predictiva y matriz de producciones.
- Tabla de símbolos con:
  - identificador;
  - tipo;
  - dirección lógica `VRAM[n]`;
  - indicador de constante;
  - valor conocido cuando puede deducirse.
- Declaración de variables:

  ```lia
  def x, y : int ;
  def promedio : float ;
  ```

- Declaración de constantes con literal inicial:

  ```lia
  const PI : real = 3.1416 ;
  ```

- `real` es un alias de `float`; ambos representan `TipoDato::REAL`.
- Validación mediante cubo semántico.
- Pilas de trabajo:
  - `PilaO`: operandos, constantes y temporales;
  - `PTipos`: tipos asociados a los operandos;
  - `POper`: operadores pendientes;
  - `PSaltos`: saltos para control de flujo futuro;
  - `AVAIL`: temporales disponibles para reutilización.
- Temporales `$T1`, `$T2`, etc.
- Cuádruplos con formato:

  ```text
  (COPER, OP1, OP2, RESULTADO)
  ```

- Asignaciones:

  ```text
  (=, $T1, _, variable)
  ```

- Entrada y salida:

  ```text
  (input, _, _, variable)
  (output, _, _, resultado)
  ```

- Historial visual del proceso de pilas.
- Reporte de errores sintácticos y semánticos.

## Tipos y booleanos

El lenguaje reconoce actualmente estos tipos:

```text
int
float / real
char
string
bool
```

Es importante diferenciar el tipo `bool` de los literales booleanos:

```text
bool   = tipo de dato
true   = literal booleano verdadero
false  = literal booleano falso
```

El tipo `bool` sí está implementado. También están implementadas las
comparaciones y operaciones lógicas que producen resultados booleanos:

```lia
def a, b : int ;
def mayor, resultado : bool ;

a = 10 ;
b = 20 ;
mayor = a > b ;
resultado = ! ( a < b ) ;
```

Sin embargo, `true` y `false` todavía no están registrados como literales
booleanos en el lexer y en las producciones de `FACT`. Por eso, actualmente
el lexer los interpreta como identificadores. Este código todavía produce un
error de identificador no declarado:

```lia
def activo : bool ;
activo = true ;
```

Mientras se implementan esos literales, se puede obtener un booleano mediante
una comparación:

```lia
def a, b : int ;
def activo : bool ;

a = 10 ;
b = 10 ;
activo = a == b ;
```

La negación lógica `!` sí funciona cuando recibe un valor de tipo `bool`:

```lia
def a, b : int ;
def activo, bloqueado : bool ;

a = 10 ;
b = 10 ;
activo = a == b ;
bloqueado = ! activo ;
```

## Operadores soportados

### Aritméticos

```text
     +    -    *    /    %
```

### Relacionales

```text
     ==   !=   <    <=   >    >=
```

### Lógicos

```text
     &&   ||   !
```

La precedencia general va de operadores aritméticos a relacionales y después
a operadores lógicos. Los paréntesis pueden utilizarse para modificar el
orden:

```lia
resultado = ! ( a < b && activo ) ;
```

## Entrada y salida

La sintaxis actual es:

```lia
input  ( id, id, ... ) ;
output ( EXPR, EXPR, ... ) ;
```

`input` recibe identificadores declarados y los coloca como destino de la
entrada. `output` recibe expresiones. Por ejemplo:

```lia
def x, y : int ;

input ( x, y ) ;
output ( x + y ) ;
```

El cuádruplo de `output` coloca el valor que se mostrará en `RESULTADO`:

```text
(output, _, _, $T2)
```

## Acciones semánticas y producciones

Las producciones sintácticas se encuentran principalmente en
[Parser.cpp](C:/Users/jashe/Desktop/Lia_Automata_II/src/Parser.cpp). Las
acciones semánticas se ejecutan desde funciones como:

```text
Parser::ejecutarAccionDeProduccion()
Parser::ejecutarAccionDeMatch()
Parser::reducirOperadoresConPrecedenciaMinima()
```

Entre las producciones y acciones integradas se encuentran:

```text
FACT -> id
FACT -> cteentera
FACT -> ctereal
FACT -> ctecaracter
FACT -> ctestring
FACT -> ( EXPR )
EXPR3 -> ! EXPR4
```

También se añadió la producción de constantes:

```text
DECLARA -> const id : TIPO = FACT ; DECLARA
```

El número utilizado para identificar un token es una convención interna del
proyecto. Actualmente un identificador usa el gramema `101`; en otra
convención académica podría usar `1000`. Lo importante es que lexer, parser y
acciones semánticas utilicen consistentemente la misma tabla.

## Pilas y cuádruplos

La pestaña **Proceso semántico** muestra una secuencia de estados. Cada fila
representa una acción como:

```text
ENTRA ↑ operando
SALE ↓ temporal
REDUCE → operando1 operador operando2
```

Las pilas se leen de izquierda a derecha:

```text
BASE → elemento1 → elemento2 → TOPE
```

El número `CONT` identifica el cuádruplo, no el valor de una variable llamada
`contador`. Por ejemplo:

```text
1  (input, _, _, x)
2  (+, x, 1, $T1)
3  (=, $T1, _, x)
```

`AVAIL` permite reutilizar temporales que ya no son necesarios, por lo que un
temporal como `$T1` puede aparecer otra vez en una operación posterior.

## Estructura principal

```text
LIA_Automatas_II/
├── CMakeLists.txt
├── README.md
├── compilar_y_ejecutar.bat
├── ejemplos/
└── src/
    ├── Lexer.cpp / Lexer.h
    ├── Parser.cpp / Parser.h
    ├── SemanticParser.cpp / SemanticParser.h
    ├── SemanticAnalysis.cpp / SemanticAnalysis.h
    ├── SemanticWindow.cpp / SemanticWindow.h
    ├── Token.cpp / Token.h
    ├── Error.cpp / Error.h
    ├── MainWindow.cpp / MainWindow.h
    └── main.cpp
```

### Archivos importantes

- `Lexer.cpp`: reconoce lexemas y genera tokens.
- `Parser.cpp`: ejecuta el análisis LL(1) y las acciones semánticas embebidas.
- `SemanticParser.cpp`: implementa tipos, símbolos, pilas, cubo semántico y
  generación de cuádruplos.
- `SemanticAnalysis.cpp`: analizador auxiliar y formateadores de texto.
- `SemanticWindow.cpp`: presenta tablas, pilas, cuádruplos y errores.
- `MainWindow.cpp`: ventana principal, editor y controles de análisis.

## Ejemplos

La carpeta [ejemplos/](C:/Users/jashe/Desktop/Lia_Automata_II/ejemplos)
contiene casos de prueba. La lista completa y la sintaxis detallada se
encuentran en [ejemplos/README.md](C:/Users/jashe/Desktop/Lia_Automata_II/ejemplos/README.md).

Algunos casos importantes:

- `01_basico_semantico.lia`: declaraciones, suma y asignación.
- `02_precedencia_parentesis.lia`: precedencia, `%` y paréntesis.
- `03_entrada_salida.lia`: `input` y `output`.
- `05_relacionales_y_logicos.lia`: comparaciones, `&&`, `||` y `!`.
- `06_constantes.lia`: constantes con `const`.
- `07_programa_completo.lia`: ejemplo integral.
- `08_operadores_relacionales.lia`: operadores relacionales.
- `09_negaciones.lia`: prueba de negaciones; actualmente debe considerarse
  un ejemplo de diagnóstico porque usa `true`, que aún no es literal válido.
- `10_unarios_logicos.lia`: negación numérica, negación lógica, `&&` y `||`;
  todos sus identificadores están declarados y debe ser un caso válido.

## Compilación en Windows

### Script del proyecto

Desde la raíz del repositorio:

```bat
compilar_y_ejecutar.bat
```

El script limpia `build/`, configura CMake, compila y ejecuta el programa.
Está configurado para rutas locales específicas de Qt y MinGW, por lo que
puede ser necesario ajustar:

```bat
C:\Qt\6.11.1\mingw_64\bin
C:\Users\jashe\Downloads\w64devkit\bin
```

### CMake manual

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build build
```

El ejecutable se genera normalmente en:

```text
build/AnalizadorLexicoLIA.exe
```

## Cómo probar un ejemplo

1. Ejecuta la aplicación.
2. Pulsa **Abrir**.
3. Selecciona un archivo `.lia` de `ejemplos/`.
4. Pulsa **Analizar**.
5. Revisa tokens, traza sintáctica, símbolos, proceso semántico,
   cuádruplos y errores.

Los comentarios del lenguaje utilizan `$`:

```lia
$ Este texto es un comentario
```

## Estado del proyecto

El flujo principal de lexer, parser, análisis semántico y generación de
cuádruplos ya está integrado y funcional para las construcciones documentadas.
La siguiente mejora pendiente relacionada con booleanos es agregar `true` y
`false` como literales reconocidos por lexer y parser. El tipo `bool`, las
comparaciones y las operaciones `!`, `&&` y `||` ya forman parte de la
implementación semántica.

## Autores

- Jasher Abraham Carrazco Vaquera
- Luis Guillermo Meléndez Mier

Jasher Abraham Carrazco Vaquera y Luis Guillermo Meléndez Mier trabajan
actualmente en esta segunda parte del analizador semántico para la materia de
Autómatas II.

## Licencia

Proyecto académico.
