# Ejemplos de prueba del flujo completo

Estos archivos están preparados para abrirse desde la aplicación y probar:

```text
Lexer -> Parser LL(1) -> acciones semánticas -> cuádruplos
```

## Casos válidos

- `01_basico_semantico.lia`: declaraciones, suma y asignaciones.
- `02_precedencia_parentesis.lia`: `+`, `*`, `%` y paréntesis.
- `03_entrada_salida.lia`: `input` con expresiones y `output` con identificadores.
- `05_relacionales_y_logicos.lia`: `<`, `>=`, `==`, `!=`, `&&`, `||` y `!`.

## Caso con errores

- `04_errores_semanticos.lia`: debe pasar la sintaxis, pero reportar:
  - identificador no declarado;
  - incompatibilidad de asignación;
  - `%` aplicado a un `REAL`.

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

La gramática actual del proyecto define:

```text
input  ( EXPR, ... ) ;
output ( id, id, ... ) ;
```

Por eso `03_entrada_salida.lia` conserva ese orden, aunque los nombres
internos `EST_WRITE` y `EST_READ` puedan parecer invertidos.
