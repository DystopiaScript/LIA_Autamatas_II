# Ejemplos de prueba del flujo completo

Estos archivos están preparados para abrirse desde la aplicación y probar:

```text
Lexer -> Parser LL(1) -> acciones semánticas -> cuádruplos
```

## Casos válidos

- `01_basico_semantico.lia`: declaraciones, suma y asignaciones.
- `02_precedencia_parentesis.lia`: `+`, `*`, `%` y paréntesis.
- `03_entrada_salida.lia`: `input` con identificadores y `output` con expresiones.
- `07_programa_completo.lia`: declaraciones, constantes, expresiones, input y output.
- `06_constantes.lia`: declaraciones con `const id : tipo = literal`.
- `05_relacionales_y_logicos.lia`: `<`, `>=`, `==`, `!=`, `&&`, `||` y `!`.
- `08_operadores_relacionales.lia`: ejemplo de `>`, `<`, `>=`, `<=`, `==` y
  `!=`, con todos los identificadores declarados.
- `09_negaciones.lia`: negación lógica `!` aplicada a variables booleanas y a
  expresiones relacionales/lógicas.
- `10_unarios_logicos.lia`: negación numérica `-`, negación lógica `!`, `&&`
  y `||`, sin utilizar todavía los literales `true` y `false`. Todos sus
  identificadores están declarados y debe ser un caso válido.
- `11_condicionales.lia`: estatutos `if` sin `else` y `if` con `else`,
  generación de `gotofalso`, `goto` y backpatching mediante `PSaltos`.

## Caso con errores

- `04_errores_semanticos.lia`: debe pasar la sintaxis, pero reportar:
  - incompatibilidad de asignación;
  - `%` aplicado a un `REAL`.
  Todos los identificadores usados están declarados para que el ejemplo se
  concentre en esos errores.

## Cómo probarlos

1. Ejecuta la aplicación.
2. Pulsa **Abrir**.
3. Selecciona uno de los archivos `.lia`.
4. Pulsa **Analizar**.
5. Revisa:
   - el panel de tokens;
   - la traza sintáctica;
   - la ventana de resultados semánticos;
   - la tabla de símbolos;
   - las pilas;
   - los cuádruplos;
   - los errores semánticos.

Los comentarios comienzan con `$`, que es la convención de comentarios
reconocida por el lexer actual.

Los estatutos condicionales siguen la forma del material:

```lia
if ( condicion ) then {
    estatutos
    else
    estatutos
}
```

La gramática actual del proyecto define:

```text
input  ( id, id, ... ) ;
output ( EXPR, ... ) ;
```

Estas formas corresponden a los estatutos convencionales: `input` recibe
variables que serán destinos de datos y `output` recibe expresiones que se
mostrarán.

En los cuádruplos, `output` coloca la expresión que se mostrará en
`RESULTADO`, por ejemplo:

```text
(output, _, _, $T2)
```

La declaración de constantes integrada utiliza:

```text
const PI : real = 3.1416;
```

El lenguaje acepta `real` como alias de `float`; ambos representan el tipo
semántico REAL. Los programas existentes que usan `float` siguen siendo
válidos.

En esta primera versión, el valor debe ser un literal compatible con el tipo.
Las constantes inicializadas mediante expresiones completas quedan para una
ampliación posterior de la gramática.
