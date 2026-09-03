# Laboratorio 1 — Decodificador de UTF-8
**Curso:** Estructura de Datos II  
**Lenguaje:** C++  
**Modalidad:** Individual  

---

## 1. Descripción general

En este laboratorio implementarán, **desde cero y sin usar bibliotecas de codificación de texto**, un programa en C++ que lea un archivo de texto codificado en **UTF-8** y decodifique manualmente su contenido byte por byte.

El objetivo no es solo "leer el archivo", sino que ustedes mismos implementen la lógica de decodificación de UTF-8: interpretación de bytes líder y de continuación, reconstrucción de puntos de código (*code points*), y detección de errores de codificación.

Este ejercicio los obliga a trabajar directamente con manipulación de bits, máquinas de estado simples y estructuras de datos para acumular resultados — habilidades centrales del curso.

---

## 2. Trasfondo técnico: UTF-8 no tiene *endianness*

Antes de comenzar, es importante entender una propiedad de UTF-8 que **justifica** por qué el procesamiento debe hacerse estrictamente byte por byte.

A diferencia de UTF-16 o UTF-32 — que representan cada *code point* como una o más unidades de **ancho fijo** (16 o 32 bits) y por lo tanto necesitan especificar en qué orden se almacenan los bytes de esa unidad (*big-endian* o *little-endian*) — **UTF-8 no tiene *endianness*, porque no se define en términos de unidades multi-byte de ancho fijo**. UTF-8 es, por diseño, una secuencia de bytes individuales de 8 bits, donde el rol de cada byte es inequívoco a partir de su propio patrón de bits (byte líder de 1, 2, 3 o 4 bytes, o byte de continuación `10xxxxxx`). No existe ambigüedad de "cuál byte es más significativo" porque nunca se reinterpreta un grupo de bytes como un entero de 16 o 32 bits almacenado en memoria; se procesa cada byte de forma independiente, en el orden en que aparece en el archivo.

**Esto tiene una consecuencia directa y obligatoria para su implementación: el archivo debe leerse y decodificarse un byte a la vez (`unsigned char` / `uint8_t`), nunca en bloques de 16 o 32 bits.**

Si, por ejemplo, leyeran el archivo agrupando 2 bytes en un entero de 16 bits, ocurrirían dos problemas distintos:

1. **Desalineación:** como UTF-8 es de ancho variable (1 a 4 bytes por *code point*, y esa longitud solo se conoce inspeccionando el byte líder), agrupar bytes a ciegas en bloques de tamaño fijo eventualmente hace que tomen la mitad de un *code point* y la mitad del siguiente, corrompiendo toda la decodificación a partir de ese punto.
2. ***Endianness* espuria:** al ensamblar 2 bytes en un entero de 16 bits, el resultado depende de cómo la máquina interprete ese ensamblado (*big-endian* vs. *little-endian* — por ejemplo, los bytes `0xC3 0xA9` producirían `0xC3A9` interpretados como *big-endian*, pero `0xA9C3` como *little-endian*, típico en procesadores x86/ARM). Ninguno de esos dos números tiene significado alguno en UTF-8 — sería un artefacto de cómo la máquina ensambla bytes en memoria, no una decodificación real. Como UTF-8 no tiene *endianness*, introducir una interpretación de 16 o 32 bits estaría inventando un problema que la codificación fue diseñada para no tener.

La forma correcta de decodificar es puramente **lógica y bit a bit sobre un solo byte a la vez**: aplicar máscaras y desplazamientos (`&`, `>>`, `<<`) para extraer los bits `x` de cada byte individual, y concatenar esos grupos de bits en el orden que UTF-8 define — sin nunca pedirle a la máquina que "ensamble" varios bytes en un entero de una vez. Por esto el procesamiento **debe** hacerse estrictamente byte por byte: no solo es la forma más simple de implementarlo, es la única forma de evitar tanto la desalineación como la dependencia espuria del *endianness* del procesador donde corre el programa.

> Nota al margen: el BOM de UTF-8 (`EF BB BF`, ver sección 4.4) existe únicamente como firma de identificación de codificación — a diferencia del BOM de UTF-16 (`FEFF`/`FFFE`), que sí indica el *endianness* de las unidades de 16 bits que le siguen. En UTF-8 el BOM no comunica ningún orden de bytes, porque no hay ninguno que comunicar.

---

## 3. Restricciones obligatorias

1. **Modo binario:** el archivo debe abrirse en modo binario (`std::ios::binary`), **no** en modo texto. Deben leer el archivo byte por byte (`unsigned char` / `uint8_t`), sin dejar que la biblioteca estándar interprete el contenido por ustedes.
2. **Prohibido usar bibliotecas o funciones que decodifiquen o manejen Unicode/UTF-8 por ustedes.** Esto incluye, pero no se limita a:
   - `<codecvt>`, `std::wstring_convert`
   - `<locale>` con facetas de conversión Unicode
   - `<uchar.h>` / `mbrtoc16`, `mbrtoc32`, `mbstowcs`, etc.
   - Cualquier biblioteca externa (ICU, utf8cpp, Boost.Locale, etc.)
   - Copiar/pegar un decodificador ya hecho de internet

   **Sí está permitido** usar la biblioteca estándar de C++ para lo que **no** sea decodificación de UTF-8: `std::vector`, `std::string` (como contenedor de bytes crudos), `std::ifstream` en modo binario, `std::cout`, contenedores STL en general, etc. Esto incluye **`std::format`** (obligatorio para el formato hexadecimal, ver sección 4.2.1) — la restricción es únicamente sobre bibliotecas de decodificación Unicode/UTF-8, no sobre formateo de salida.
3. El programa debe recibir la ruta del archivo como argumento de línea de comandos (`argv[1]`).
4. **Estándar del lenguaje:** el laboratorio debe compilarse con **C++20** (`-std=c++20`). Se espera que usen `std::format` (ver sección 4.2.1) para el formato hexadecimal `U+XXXX`. Antes de comenzar, verifiquen con `g++ --version` que su compilador soporta `<format>` completamente (GCC 13 o superior); si su entorno de trabajo tiene una versión más antigua, deben actualizarla o usar el compilador disponible en los laboratorios de la universidad.

---

## 4. Especificación funcional

### 4.1 Decodificación

UTF-8 codifica cada punto de código Unicode usando 1 a 4 bytes, según estos patrones de bits:

| Rango de code point | Bytes | Patrón binario |
|---|---|---|
| U+0000 – U+007F | 1 | `0xxxxxxx` |
| U+0080 – U+07FF | 2 | `110xxxxx 10xxxxxx` |
| U+0800 – U+FFFF | 3 | `1110xxxx 10xxxxxx 10xxxxxx` |
| U+10000 – U+10FFFF | 4 | `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` |

Cada byte de continuación tiene el patrón `10xxxxxx`. Deben extraer los bits `x`, concatenarlos en el orden correcto y reconstruir el valor entero del *code point*.

#### 4.1.1 Pseudocódigo del algoritmo

La lógica central se reduce a examinar el **primer byte** de cada secuencia y decidir, según el rango en el que cae, cuántos bytes de continuación esperar y cómo extraer los bits de cada uno. A continuación un pseudocódigo de referencia (no es código C++ literal; ustedes deciden la implementación concreta):

```
leer_archivo_en_buffer_de_bytes()
offset = 0
si buffer comienza con EF BB BF:
    offset = 3   // saltar BOM

mientras offset < longitud(buffer):
    b1 = buffer[offset]

    si (b1 & 0x80) == 0x00:                     // patrón 0xxxxxxx
        code_point = b1
        bytes_usados = 1

    si_no si (b1 & 0xE0) == 0xC0:                // patrón 110xxxxx
        bytes_usados = 2
        si offset + 1 >= longitud(buffer):
            reportar_error("secuencia incompleta", offset)
            offset = offset + 1
            continuar
        b2 = buffer[offset + 1]
        si (b2 & 0xC0) != 0x80:                  // no es 10xxxxxx
            reportar_error("byte de continuación esperado, no encontrado", offset)
            offset = offset + 1
            continuar
        code_point = ((b1 & 0x1F) << 6) | (b2 & 0x3F)

    si_no si (b1 & 0xF0) == 0xE0:                // patrón 1110xxxx
        bytes_usados = 3
        si offset + 2 >= longitud(buffer):
            reportar_error("secuencia incompleta", offset)
            offset = offset + 1
            continuar
        b2 = buffer[offset + 1]
        b3 = buffer[offset + 2]
        si (b2 & 0xC0) != 0x80 o (b3 & 0xC0) != 0x80:
            reportar_error("byte de continuación esperado, no encontrado", offset)
            offset = offset + 1
            continuar
        code_point = ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F)

    si_no si (b1 & 0xF8) == 0xF0:                // patrón 11110xxx
        bytes_usados = 4
        si offset + 3 >= longitud(buffer):
            reportar_error("secuencia incompleta", offset)
            offset = offset + 1
            continuar
        b2 = buffer[offset + 1]
        b3 = buffer[offset + 2]
        b4 = buffer[offset + 3]
        si (b2 & 0xC0) != 0x80 o (b3 & 0xC0) != 0x80 o (b4 & 0xC0) != 0x80:
            reportar_error("byte de continuación esperado, no encontrado", offset)
            offset = offset + 1
            continuar
        code_point = ((b1 & 0x07) << 18) | ((b2 & 0x3F) << 12) | ((b3 & 0x3F) << 6) | (b4 & 0x3F)

    si_no si (b1 & 0xC0) == 0x80:                 // patrón 10xxxxxx: byte de continuación "huérfano"
        reportar_error("byte de continuación inesperado", offset)
        offset = offset + 1
        continuar

    si_no:                                        // 0xF8-0xFF: byte líder inválido (ver bono, sección 4.3)
        reportar_error("byte líder inválido", offset)
        offset = offset + 1
        continuar

    guardar(code_point, bytes_usados)   // acumular en el vector de code points (sección 4.5)
    offset = offset + bytes_usados
```

**Notas sobre el pseudocódigo:**
- Cada rama examina el primer byte con una máscara (`&`) distinta para aislar los bits fijos del patrón (`0x80`, `0xE0`, `0xF0`, `0xF8`) y compararlos contra el valor esperado — así se determina a qué rango de la tabla anterior pertenece, sin necesidad de comparar contra rangos numéricos de *code points* todavía.
- Los bits útiles de cada byte se extraen con una máscara que aísla los bits `x` (`0x1F`, `0x0F`, `0x07` para el byte líder; `0x3F` para bytes de continuación) y se combinan con desplazamientos (`<<`) y OR (`|`) — este es exactamente el tipo de operación bit a bit sobre bytes individuales descrito en la sección 2 (sin ensamblar enteros de 16/32 bits directamente desde el buffer).
- Cuando una secuencia resulta inválida (continuación faltante, EOF prematuro, byte huérfano), el pseudocódigo avanza `offset` en **1 solo byte** antes de continuar — esto evita que un único byte corrupto descarte más contenido válido del necesario, y es consistente con el requisito de la sección 4.3 de seguir procesando tras un error.
- `reportar_error(...)` aquí significa *almacenar* el error (offset + tipo) en la estructura correspondiente, no imprimirlo inmediatamente — recuerden la separación decodificación/reporte de la sección 4.5.

### 4.2 Reporte por code point

Por cada *code point* válido decodificado, el programa debe reportar:

- Si el *code point* está en el rango **ASCII imprimible** (`0x20` a `0x7E` inclusive), **imprimir el carácter** tal cual.
- En **cualquier otro caso** (incluyendo saltos de línea, tabulaciones, y todo code point fuera del rango ASCII imprimible), reportar el código en el formato `U+XXXX` (hexadecimal, mínimo 4 dígitos, mayúsculas). Por ejemplo: `U+00E9` para "é", `U+1F600` para un emoji.

> **Nota:** no se les pide implementar una tabla de "categorías Unicode" (letra, símbolo, control, etc.). La regla de "imprimible" para este laboratorio es únicamente el rango ASCII 0x20–0x7E. Todo lo demás se reporta como código, sin excepción.

#### 4.2.1 Formato hexadecimal con `std::format` (obligatorio, C++20)

Deben usar `std::format` (header `<format>`) para generar el formato `U+XXXX`:

```cpp
std::string codigo = std::format("U+{:04X}", codepoint);
```

`{:04X}` produce hexadecimal en mayúsculas con un mínimo de 4 dígitos (rellenando con ceros a la izquierda); para *code points* de 4 bytes (mayores a `0xFFFF`) el resultado simplemente ocupa más de 4 dígitos, lo cual es correcto.

**Advertencia de compatibilidad:** `<format>` es parte del estándar C++20, pero el soporte en compiladores varía. GCC lo soporta completamente recién desde la versión 13; versiones más antiguas (por ejemplo GCC 11 o 12, presentes en algunas instalaciones de Ubuntu LTS) pueden no tenerlo o tenerlo incompleto. **Verifiquen con `g++ --version` antes de empezar** y, si su entorno no lo soporta, actualicen su compilador o usen el entorno provisto por la universidad — `std::format` es un requisito de este laboratorio, no una alternativa opcional.

### 4.3 Manejo de errores

UTF-8 mal formado debe ser **detectado y reportado explícitamente**, no ignorado ni saltado en silencio. Como mínimo, deben detectar y reportar estos dos casos, indicando el **byte offset** (posición en el archivo, comenzando en 0) donde ocurre el error:

1. **Byte de continuación inesperado:** aparece un byte con patrón `10xxxxxx` donde se esperaba un byte líder.
2. **Secuencia incompleta / truncada:** un byte líder indica que le siguen N bytes de continuación, pero el archivo termina antes (EOF) o alguno de esos bytes no tiene el patrón `10xxxxxx` esperado.

Al encontrar un error, el programa debe:
- Imprimir un mensaje de error claro indicando el offset y el tipo de problema.
- Continuar procesando el resto del archivo desde el siguiente byte razonable (no debe terminar el programa abruptamente ante el primer error).

**Bono opcional (no obligatorio, puntos extra):** detectar bytes líder inválidos (`0xF8`–`0xFF`, que nunca son válidos en UTF-8) y detectar codificaciones "sobrelargas" (*overlong encodings*, por ejemplo codificar `'A'` con 2 bytes en vez de 1).

### 4.4 BOM (Byte Order Mark)

Si el archivo comienza con los tres bytes `EF BB BF` (BOM de UTF-8), el programa debe detectarlos y **omitirlos silenciosamente** (no reportarlos como carácter ni como error). Si el archivo no tiene BOM, no hacer nada especial.

### 4.5 Separación de responsabilidades (requisito estructural)

Para separar la fase de *decodificación* de la fase de *reporte* (una buena práctica de diseño y el enfoque que corresponde a un curso de Estructura de Datos):

1. Primero, decodifiquen **todo** el archivo y almacenen cada *code point* válido en un `std::vector<uint32_t>`.
2. Los errores encontrados durante la decodificación también deben almacenarse (por ejemplo, en un `std::vector` de una struct simple con offset y tipo de error), no imprimirse directamente en esa fase.
3. Después de decodificar todo el archivo, recorran el vector de *code points* para generar el reporte carácter-por-carácter (sección 4.2), y luego impriman los errores acumulados (sección 4.3) y el resumen final (sección 4.6).

### 4.6 Resumen final

Al terminar de procesar el archivo, el programa debe imprimir un resumen con:

- Número total de bytes leídos del archivo.
- Número total de *code points* válidos decodificados.
- Conteo de *code points* según su longitud de codificación original: cuántos de 1 byte, cuántos de 2 bytes, cuántos de 3 bytes, cuántos de 4 bytes.
- Número total de errores detectados.

---

## 5. Formato de salida sugerido

```
=== Contenido decodificado ===
H
o
l
a
U+00E9
...

=== Errores detectados ===
[offset 42] Byte de continuación inesperado (0x91) sin byte líder previo.
[offset 87] Secuencia incompleta: se esperaban 2 bytes de continuación, EOF alcanzado.

=== Resumen ===
Bytes totales:            120
Code points válidos:      98
  - 1 byte:                80
  - 2 bytes:               15
  - 3 bytes:               3
  - 4 bytes:               0
Errores detectados:        2
```

El formato exacto (espaciado, encabezados) puede variar; lo importante es que la información esté presente y sea legible.

---

## 6. Entregables

1. Código fuente en C++ (`.cpp` / `.h` según corresponda), compilable con `g++ -std=c++20`.
2. Un archivo `README.md` breve explicando cómo compilar (con `-std=c++20`) y ejecutar el programa.
3. **No es necesario** entregar archivos de prueba; el profesor probará el programa con sus propios archivos, incluyendo casos con caracteres multibyte válidos y casos con UTF-8 mal formado intencionalmente.

---

## 7. Rúbrica de evaluación

| Criterio | Puntos |
|---|---|
| Lectura correcta del archivo en modo binario, byte por byte | 10 |
| Decodificación correcta de secuencias de 1 byte (ASCII) | 15 |
| Decodificación correcta de secuencias de 2, 3 y 4 bytes | 25 |
| Reporte correcto: carácter imprimible vs. `U+XXXX` | 15 |
| Detección y reporte de byte de continuación inesperado | 10 |
| Detección y reporte de secuencia incompleta / truncada | 10 |
| Manejo correcto de BOM | 5 |
| Separación decodificación/reporte (vector de code points + vector de errores) | 5 |
| Resumen final completo y correcto | 5 |
| **Bono:** detección de bytes líder inválidos y/o codificaciones sobrelargas | +5 |
| **Total** | **100 (+5 bono)** |

**Penalización:** el uso de cualquier biblioteca o función prohibida (sección 3, punto 2) resulta en **0 puntos** en el laboratorio, independientemente de que el resto funcione correctamente.

---

## 8. Preguntas frecuentes (anticipadas)

**¿Puedo usar `std::string` para almacenar los bytes leídos?**
Sí, siempre que la trates como un contenedor de bytes crudos (por ejemplo, leyendo con `std::ifstream::read` en modo binario) y no como texto ya decodificado.

**¿Qué hago si el archivo tiene una secuencia de 4 bytes (emoji, etc.)?**
Se reporta como `U+XXXX` con el código completo del *code point*, siguiendo la misma regla de la sección 4.2 (fuera de rango ASCII imprimible → se reporta el código).

**¿Qué pasa con saltos de línea (`\n`) y tabulaciones?**
Se reportan como código (`U+000A`, `U+0009`), no como carácter, porque no están en el rango 0x20–0x7E.

**¿Puedo usar `printf("%x", ...)` en vez de `std::format`?**
No. El laboratorio requiere C++20 y `std::format` para el formato hexadecimal (sección 4.2.1) — no se acepta `printf` ni manipuladores de `<iostream>` como sustituto para ese formato.

**¿Puedo compilar con C++17 si mi código no usa ninguna característica nueva de C++20?**
No. El estándar requerido es **C++20** (`-std=c++20`) para todas las entregas, independientemente de qué características usen — principalmente porque `std::format` es obligatorio y solo está disponible desde C++20.
