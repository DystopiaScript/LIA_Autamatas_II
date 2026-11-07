# Resumen de Cambios: Fase Semántica, Pilas Numéricas, Intérprete y Nueva Interfaz

**Fecha:** 6 de noviembre de 2026  
**Rama Git:** `Jasher_Correciones`  
**Proyecto:** Compilador LIA (Lenguajes y Autómatas II)

---

## 1. Contexto y Objetivos del Trabajo

En esta sesión se abordaron tres requerimientos fundamentales para la entrega del proyecto:

1. **Corrección de la ejecución de estatutos condicionales (`IF-THEN` e `IF-THEN-ELSE`):**
   - Anteriormente, el compilador propagaba textualmente las asignaciones conforme leía el código fuente. Esto provocaba que en condicionales consecutivos o ramas `if / else`, el valor final en la tabla de símbolos reflejara el último texto leído y no la rama efectivamente evaluada.
   - Se requería que se evaluaran las expresiones en tiempo de compilación y se respetaran los saltos `gotofalso` y `goto`.

2. **Requerimiento del profesor sobre Pilas y Cuádruplos Numéricos:**
   - *Instrucción del profesor:* *"Dentro de las especificaciones para el proyecto les comenté que los cuádruplos y las pilas, solamente van a manejar valores numéricos..."*
   - Las pilas internas (`PilaO`, `PTipos`, `POper`, `PSaltos`, `temporalesDisponibles`) debían operar estrictamente con enteros (`int`), sin mezclar strings en la memoria de trabajo del compilador.

3. **Rediseño Didáctico de la Interfaz Visual (`SemanticWindow`):**
   - Se separaron los cuádruplos y el historial de pilas en pestañas independientes para evitar que la información estuviera amontonada.
   - Se implementó la vista de **cinco columnas verticales** (`OPERADORES`, `OPERANDOS`, `TIPOS`, `SALTOS`, `AVAILS`) con tarjetas estructuradas paso a paso (`ESTADO INICIAL`, movimientos individuales y `ESTADO FINAL`).
   - Se añadió un interruptor para alternar entre nombres amigables y códigos numéricos internos.

---

## 2. Reutilización del Mini-Intérprete para los Estatutos Restantes

> **¿Podemos usar este mini-intérprete en los siguientes estatutos (e.g. `while`, `do-while`, etc.)?**
>
> **¡SÍ, TOTALMENTE!**

El intérprete implementado en [`src/SemanticParser.cpp`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticParser.cpp) (`Semantica::Interprete::ejecutarCuadruplos`) fue diseñado de forma genérica sobre la máquina de cuádruplos:

- Funciona con un Program Counter (`pc`) que recorre el vector de cuádruplos.
- Soporta asignación (`=`), operadores aritméticos (`+`, `-`, `*`, `/`, `%`), relacionales (`<`, `<=`, `>`, `>=`, `==`, `!=`), lógicos (`&&`, `||`, `!`) y saltos (`gotofalso`, `goto`).
- **Por qué servirá directamente para ciclos (`while`, `do-while`, `for`):**
  En teoría de compiladores, un ciclo `while` se descompone en exactamente los mismos cuádruplos que un `if`:
  1. Etiqueta de inicio de condición.
  2. Evaluación de la expresión booleana.
  3. `gotofalso <condicion> _ <fin_del_ciclo>` (si es falsa, salta al final).
  4. Bloque de estatutos del cuerpo del ciclo.
  5. `goto _ _ <inicio_condicion>` (salto incondicional para volver a evaluar).
  6. Etiqueta de fin del ciclo (destino del `gotofalso`).
  
Como el intérprete ya sabe ejecutar `gotofalso` hacia adelante y `goto` hacia atrás o hacia adelante, **cuando se añadan las acciones semánticas de `while` en el Parser, el intérprete ejecutará los ciclos automáticamente sin modificar su lógica base**.

---

## 3. Arquitectura Numérica de las Pilas y Cuádruplos

### 3.1 ¿Por qué se hizo este cambio? (El problema anterior)
Anteriormente, el compilador almacenaba cadenas de texto (`std::string`) dentro de las pilas de trabajo:
- En `POper` se guardaban signos de texto como `"+"` o `"=="`.
- En `PTipos` se guardaban palabras como `"int"` o `"real"`.
- En `PilaO` se guardaban cadenas como `"edad"`, `"$T1"` o `"18"`.

El profesor especificó expresamente:
> *"Dentro de las especificaciones para el proyecto les comenté que los cuádruplos y las pilas, **solamente van a manejar valores numéricos**, por favor revisen lo que están haciendo..."*

En teoría de compiladores y máquinas de pila reales, una pila de ejecución no almacena texto arbitrario, sino **códigos numéricos de operación (opcodes) y direcciones virtuales de memoria**.

---

### 3.2 ¿Cómo funciona el nuevo esquema 100% numérico?

Todas las pilas internas se convirtieron a `std::stack<int>`. **Dentro de la memoria del compilador no entra ningún `string`**.

Para evitar colisiones entre tokens del léxico (gramemas 100–131), no terminales (1–29) y códigos de error (500–603), se establecieron los siguientes rangos numéricos discretos:

```
[0 ........ 1999]  -> Direcciones de memoria RAM virtual de variables declaradas
[2001 ..... 2999]  -> Temporales de la pila AVAIL ($T1 = 2001, $T2 = 2002, ..., $T20 = 2020)
[3001 ..... 3999]  -> Constantes y literales numéricos/cadenas registrados en tabla
[301 ...... 305]   -> Códigos numéricos de tipos de datos en PTipos
[105 ...... 128]   -> Tokens terminales del léxico como operadores en POper
[1001 .... 1005]   -> Operadores intermedios (NEG, gotofalso, goto, input, output)
```

#### A) Pila de Operadores (`POper` $\rightarrow$ `std::stack<int>`):
Reutiliza directamente los **números de token del léxico** ya existentes:
- Si el código tiene `+`, entra el entero **`105`**.
- Si tiene `-` (binario), entra el entero **`106`**.
- Si tiene `*`, entra el entero **`107`**.
- Si tiene `==`, entra el entero **`110`**.
- Si tiene `(`, entra el entero **`119`** (actúa como marca de fondo falso).
- Para operadores intermedios generados por el compilador:
  - `NEG` (menos unario) $\rightarrow$ **`1001`**.
  - `gotofalso` $\rightarrow$ **`1002`**.
  - `goto` $\rightarrow$ **`1003`**.
  - `input` $\rightarrow$ **`1004`**.
  - `output` $\rightarrow$ **`1005`**.

#### B) Pila de Tipos (`PTipos` $\rightarrow$ `std::stack<int>`):
Se asignó la serie en el rango de los `300`:
- `int` $\rightarrow$ **`301`**
- `real` $\rightarrow$ **`302`**
- `char` $\rightarrow$ **`303`**
- `bool` $\rightarrow$ **`304`**
- `string` $\rightarrow$ **`305`**

#### C) Pila de Operandos (`PilaO` $\rightarrow$ `std::stack<int>`):
Para no confundir una dirección RAM con un temporal o un número literal, se dividió por rangos:
1. **Variables (`0 .. 1999`)**:
   - Cada variable tiene su índice de memoria virtual asignado en orden de declaración (`VRAM[0]`, `VRAM[1]`, etc.).
   - `edad` es la dirección **`0`**.
   - `resultado` es la dirección **`1`**.
   - A `PilaO` entra el entero **`0`** o **`1`**.
2. **Temporales `$T` (`2001 .. 2999`)**:
   - Provienen de la pila `AVAIL` (`temporalesDisponibles`).
   - `$T1` es el entero **`2001`**, `$T2` es **`2002`**, ..., `$T20` es **`2020`**.
3. **Constantes y Literales (`3001 .. 3999`)**:
   - Si en el código aparece el número literal `18`, el compilador lo registra en `tablaLiterales` y le asigna un ID único (ej. `3001`).
   - A `PilaO` entra el entero **`3001`**.
   - *Ventaja:* Esto previene el clásico bug donde el número literal `18` se confundiría con la variable alojada en la dirección RAM `18`.

#### D) Pila de Saltos (`PSaltos` $\rightarrow$ `std::stack<int>`):
Guarda exclusivamente los números de cuádruplo pendientes de rellenar (*backpatching*), por ejemplo el cuádruplo **`3`** o **`6`**.

#### E) Pila de Temporales Disponibles (`AVAIL` $\rightarrow$ `std::stack<int>`):
Se inicializa al inicio de cada análisis con 20 temporales disponibles (`2020` en el fondo hasta `2001` en el tope). Al ocuparse uno, se hace `POP`; al liberarse, se hace `PUSH` para ser reutilizado.

---

### 3.3 Representación Interna de los Cuádruplos
Anteriormente un cuádruplo contenía solo texto: `["==", "edad", "18", "$T1"]`.  
Ahora el `struct Cuadruplo` contiene campos numéricos primarios:

```cpp
struct Cuadruplo {
    int coperNum; // Código de operador (ej. 110 para '==')
    int op1Num;   // Operando 1 (ej. 0 para variable en RAM[0] 'edad')
    int op2Num;   // Operando 2 (ej. 3001 para el literal '18')
    int resNum;   // Resultado (ej. 2001 para el temporal $T1)
    
    // Campos string auxiliares generados únicamente para visualización en GUI
    std::string coper, op1, op2, res;
};
```

---

### 3.4 ¿Por qué en la interfaz se leen nombres y cómo defenderlo ante el profesor?
Para no obligar al usuario a descifrar una matriz de números en pantalla, la ventana (`SemanticWindow`) retransforma internamente los códigos a nombres amigables (`edad`, `==`, `int`, `$T1`).

Sin embargo, para comprobar al profesor que **la compilación opera de verdad con enteros**:
1. En la pestaña **`Historial de Pilas`**, en la esquina superior derecha, se agregó la casilla:  
   **`[x] Mostrar códigos numéricos internos`**
2. Al activar esa casilla, toda la interfaz cambia en vivo y muestra los enteros almacenados:
   - `! (116)`
   - `edad (RAM[0])`
   - `int (301)`
   - `t1 (2001)`
   - `18 (3001)`
3. Esto demuestra que la arquitectura cumple al pie de la letra con el manejo numérico exigido.

---

| Elemento | Rango / Códigos | Descripción |
| :--- | :--- | :--- |
| **Variables en RAM** | `0 .. 1999` | Dirección virtual asignada en orden de declaración (`VRAM[0]`, `VRAM[1]`, etc.). |
| **Temporales (`AVAIL`)** | `2001 .. 2999` | Códigos de variables temporales. `$T1 = 2001`, `$T2 = 2002`, ..., `$T20 = 2020`. |
| **Literales / Constantes** | `3001 .. 3999` | Números, booleanos o cadenas registrados en la tabla de literales. |
| **Tipos (`PTipos`)** | `301 .. 305` | `301` = `int`, `302` = `real`, `303` = `char`, `304` = `bool`, `305` = `string`. |
| **Operadores (`POper`)** | Códigos terminales `105 .. 128` | Reutiliza los tokens del lexer (`105` = `+`, `109` = `=`, `110` = `==`, `119` = `(`, etc.). |
| **Operadores Intermedios** | `1001 .. 1005` | `1001` = `NEG`, `1002` = `gotofalso`, `1003` = `goto`, `1004` = `input`, `1005` = `output`. |

### Estructuras en Memoria:
- `std::stack<int> PilaO`: Pila de operandos con códigos de RAM, temporales o literales.
- `std::stack<int> PTipos`: Pila de tipos numéricos.
- `std::stack<int> POper`: Pila de operadores y delimitadores numéricos.
- `std::stack<int> PSaltos`: Pila de números de cuádruplos pendientes de rellenar (*backpatching*).
- `std::stack<int> temporalesDisponibles`: Pila de temporales disponibles (`AVAIL`), inicializada con `t1` a `t20` (`2001 .. 2020`).

---

## 4. Mapeo de Tokens y Códigos para las 18 Acciones Semánticas

A continuación se detalla la relación directa entre los números de token generados por el analizador léxico, los códigos intermedios utilizados en las pilas numéricas y cada una de las 18 acciones semánticas de expresiones:

### 4.1 Catálogo de Tokens del Léxico y Códigos de Operadores

| Token Léxico / Operador | Código Numérico | Descripción | Uso en Pilas / Acciones |
| :--- | :---: | :--- | :--- |
| `IDENTIFICADOR` | `100` | Nombre de variable | Acción 1 (apila RAM `0..1999` en `PilaO`) |
| `CTE_ENTERA` | `101` | Literal entero (ej. `18`) | Acción 1 (apila en `PilaO` y `PTipos=301`) |
| `CTE_REAL` | `102` | Literal real (ej. `3.14`) | Acción 1 (apila en `PilaO` y `PTipos=302`) |
| `CTE_NOTACION` | `127` | Literal científico (ej. `1e-3`) | Acción 1 (apila en `PilaO` y `PTipos=302`) |
| `CTE_CARACTER` | `103` | Literal carácter (ej. `'a'`) | Acción 1 (apila en `PilaO` y `PTipos=303`) |
| `CTE_STRING` | `104` | Literal cadena (ej. `"hola"`) | Acción 1 (apila en `PilaO` y `PTipos=305`) |
| `+` | `105` | Suma aritmética | Acción 9 (push `POper`), Acción 10 (reduce) |
| `-` (binario) | `106` | Resta aritmética | Acción 9 (push `POper`), Acción 10 (reduce) |
| `*` | `107` | Multiplicación | Acción 7 (push `POper`), Acción 8 (reduce) |
| `/` | `108` | División | Acción 7 (push `POper`), Acción 8 (reduce) |
| `%` | `128` | Módulo / Residuo | Acción 7 (push `POper`), Acción 8 (reduce) |
| `=` | `109` | Operador de asignación | EST_ASIG (genera cuádruplo de asignación) |
| `==` | `110` | Igualdad relacional | Acción 11 (push `POper`), Acción 12 (reduce) |
| `<` | `111` | Menor que | Acción 11 (push `POper`), Acción 12 (reduce) |
| `<=` | `112` | Menor o igual que | Acción 11 (push `POper`), Acción 12 (reduce) |
| `>` | `113` | Mayor que | Acción 11 (push `POper`), Acción 12 (reduce) |
| `>=` | `114` | Mayor o igual que | Acción 11 (push `POper`), Acción 12 (reduce) |
| `!=` | `115` | Diferente de | Acción 11 (push `POper`), Acción 12 (reduce) |
| `!` | `116` | Negación lógica NOT | Acción 13 (push `POper`), Acción 14 (reduce) |
| `&&` | `117` | Conjunción lógica AND | Acción 15 (push `POper`), Acción 16 (reduce) |
| `\|\|` | `118` | Disyunción lógica OR | Acción 17 (push `POper`), Acción 18 (reduce) |
| `(` | `119` | Paréntesis de apertura | Acción 5 (fondo falso en `POper`) |
| `)` | `120` | Paréntesis de cierre | Acción 6 (elimina fondo falso en `POper`) |
| `NEG` (unario) | `1001` | Menos unario | Acción 2 (push `POper`), Acciones 3/4 (reduce) |
| `gotofalso` | `1002` | Salto condicional si falso | Condicionales / Ciclos (salto a rama falsa) |
| `goto` | `1003` | Salto incondicional | Condicionales / Ciclos (salto a fin de bloque) |
| `input` | `1004` | Entrada estándar | Cuádruplo `input _ _ id` |
| `output` | `1005` | Salida estándar | Cuádruplo `output _ _ expr` |

---

### 4.2 Tabla Detallada de las 18 Acciones Semánticas

| Acción | Token / Regla Asociada | Códigos involucrados | Pilas Afectadas | Operación Semántica y Cuádruplo Generado |
| :---: | :--- | :---: | :--- | :--- |
| **1** | Terminal en `<FACT>` (`id`, constantes) | `100..104`, `127` | `PilaO`, `PTipos` | PUSH operando (RAM o literal) y PUSH su tipo numérico (`301..305`). |
| **2** | `-` unario en `<EXPR3>` | Token `106` $\rightarrow$ Código `1001` | `POper` | PUSH operador `NEG` (`1001`) a `POper`. |
| **3** | Reducción de `-` unario válido | Código `1001` | `PilaO`, `PTipos`, `POper` | Valida tipo numérico (`301`/`302`), genera cuádruplo `NEG op1 _ temp` y PUSH temporal a `PilaO`. |
| **4** | Error en `-` unario | Código `1001` | Ninguna (error) | Si el tipo en tope de `PTipos` es `char` (`303`) o `string` (`305`), detiene con error semántico. |
| **5** | `(` en `<FACT>` $\rightarrow$ `( EXPR )` | Token `119` | `POper` | PUSH marca de fondo falso `(` (`119`) en `POper` para proteger precedencias. |
| **6** | `)` en `<FACT>` $\rightarrow$ `( EXPR )` | Token `120` | `POper` | Reduce operadores pendientes hasta encontrar `(` (`119`) y hace POP del fondo falso. |
| **7** | `*`, `/`, `%` en `<TERM'>` | Tokens `107`, `108`, `128` | `POper` | PUSH operador de alta precedencia (`*`, `/`, `%`) en `POper`. |
| **8** | Reducción de `*`, `/`, `%` | Tokens `107`, `108`, `128` | `PilaO`, `PTipos`, `POper`, `AVAIL` | POP 2 operandos y 2 tipos, consulta cubo semántico, genera `coper op1 op2 temp` y PUSH temporal. |
| **9** | `+`, `-` en `<EXPR5'>` | Tokens `105`, `106` | `POper` | PUSH operador de precedencia media (`+`, `-`) en `POper`. |
| **10** | Reducción de `+`, `-` | Tokens `105`, `106` | `PilaO`, `PTipos`, `POper`, `AVAIL` | Igual a la acción 8: resuelve suma/resta con tipos numéricos y genera cuádruplo. |
| **11** | Operador relacional en `<OPREL>` | Tokens `110..115` | `POper` | PUSH operador relacional (`==`, `<`, `<=`, `>`, `>=`, `!=`) a `POper`. |
| **12** | Reducción relacional | Tokens `110..115` | `PilaO`, `PTipos`, `POper`, `AVAIL` | Reduce comparación; consulta cubo semántico y genera cuádruplo con resultado tipo `bool` (`304`). |
| **13** | `!` en `<EXPR3>` | Token `116` | `POper` | PUSH operador de negación lógica `!` (`116`) a `POper`. |
| **14** | Reducción de `!` | Token `116` | `PilaO`, `PTipos`, `POper`, `AVAIL` | POP operando y tipo (exige `bool` `304`), genera cuádruplo `! op _ temp` con tipo `bool` (`304`). |
| **15** | `&&` en `<EXPR2'>` | Token `117` | `POper` | PUSH operador lógico `&&` (`117`) a `POper`. |
| **16** | Reducción de `&&` | Token `117` | `PilaO`, `PTipos`, `POper`, `AVAIL` | Exige `bool && bool`, genera cuádruplo `&& op1 op2 temp` y apila tipo booleano (`304`). |
| **17** | `\|\|` en `<EXPR'>` | Token `118` | `POper` | PUSH operador lógico `\|\|` (`118`) a `POper`. |
| **18** | Reducción de `\|\|` | Token `118` | `PilaO`, `PTipos`, `POper`, `AVAIL` | Exige `bool \|\| bool`, genera cuádruplo `\|\| op1 op2 temp` y apila tipo booleano (`304`). |

---

## 5. Cambios en la Interfaz Gráfica (`SemanticWindow`)

En [`src/SemanticWindow.h`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticWindow.h) y [`src/SemanticWindow.cpp`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticWindow.cpp):

1. **Pestañas organizadas:**
   - **Tabla de símbolos**: Variables, tipo, dirección VRAM, estado de inicialización y valor final calculado.
   - **Constantes**: Tabla de constantes fijas declaradas.
   - **Cuádruplos**: Tabla independiente con `CONT`, `COPER`, `OP1`, `OP2` y `RESULTADO`.
   - **Historial de Pilas**: Vista de 5 columnas dedicada.
   - **Errores semánticos**: Panel de incidencias.

2. **Diseño de las Cinco Columnas de Pilas:**
   - Columnas: `OPERADORES`, `OPERANDOS`, `TIPOS`, `SALTOS`, `AVAILS`.
   - Cada columna cuenta con su propio banner y scroll vertical individual.
   - Las tarjetas muestran únicamente los pasos que afectaron a esa pila específica, indicando la acción (`PUSH`, `POP`, `POP fondo falso '('`) y el contenido de `Tope a base:`.
   - Si una pila no registra cambios (como `SALTOS` cuando no hay condicionales), muestra:
     ```text
     Sin movimientos en esta pila.
     --- ESTADO FINAL ---
     Tope a base:
       (vacia)
     Cambios registrados: 0
     ```

3. **Modo Numérico vs Simbólico:**
   - Se añadió la casilla `[ ] Mostrar códigos numéricos internos` en la esquina superior derecha.
   - Por defecto muestra nombres amigables (`varInt1`, `int`, `!`, `t1`).
   - Al marcar la casilla, se visualizan los enteros reales almacenados (`varInt1 (RAM[0])`, `int (301)`, `! (116)`, `t1 (2001)`), permitiendo demostrar al profesor la implementación numérica interna.

---

## 6. Archivos Modificados

- [`src/Parser.cpp`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/Parser.cpp): Llama al intérprete semántico al final del análisis si no hay errores y gestiona operadores numéricos en `POper`.
- [`src/SemanticParser.h`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticParser.h): Constantes numéricas, firmas del intérprete y campos numéricos en `Cuadruplo` y `HistorialPilas`.
- [`src/SemanticParser.cpp`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticParser.cpp): Lógica de pilas numéricas, reducción de operaciones, backpatching, pool de AVAIL e intérprete de cuádruplos.
- [`src/SemanticAnalysis.cpp`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticAnalysis.cpp): Sobrecargas para representar pilas numéricas en texto.
- [`src/SemanticWindow.h`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticWindow.h): Definición de la nueva interfaz con slots para el toggle numérico.
- [`src/SemanticWindow.cpp`](file:///c:/Users/jashe/Desktop/Lia_Automata_II/src/SemanticWindow.cpp): Renderizado de las 5 columnas verticales y separación de pestañas.

---

## 7. Cómo Compilar y Probar

Desde PowerShell en la carpeta raíz del proyecto:

```powershell
$env:Path = 'C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;' + $env:Path
cmake --build build
.\build\AnalizadorLexicoLIA.exe
```

O haciendo doble clic en el script `compilar_y_ejecutar.bat`.

