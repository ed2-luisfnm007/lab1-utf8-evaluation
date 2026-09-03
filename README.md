# Evaluación práctica — Decodificación manual de UTF-8

**Curso:** Estructura de Datos II  
**Duración sugerida:** 60 minutos  
**Lenguaje:** C++20  
**Sistema de pruebas:** doctest + CMake/CTest

## Objetivo

Complete las funciones marcadas con `TODO` en `src/utf8_evaluation.cpp`. Las estructuras, declaraciones y pruebas unitarias forman parte del código inicial.

La evaluación mide comprensión de bytes, máscaras, desplazamientos, UTF-8 de ancho variable, offsets, recuperación ante errores y separación entre decodificación y presentación.

## Reglas

1. Modifique únicamente `src/utf8_evaluation.cpp`, salvo indicación del profesor.
2. No modifique las pruebas, las declaraciones públicas ni `CMakeLists.txt`.
3. No use bibliotecas ni funciones que decodifiquen Unicode/UTF-8 automáticamente.
4. Se permite la biblioteca estándar para archivos, vectores, cadenas y formato.
5. `representar_codepoint` debe usar `std::format` para producir `U+XXXX`.
6. La decodificación debe almacenar resultados y errores; no debe imprimirlos.
7. Después de un error, avance un byte y continúe desde allí.
8. No codifique respuestas especiales para los datos visibles de las pruebas.

## Archivos

```text
UTF8_Evaluation/
├── CMakeLists.txt
├── README.md
├── include/
│   └── utf8_evaluation.hpp
├── src/
│   └── utf8_evaluation.cpp       # archivo que debe completar
├── tests/
│   └── tests.cpp
└── third_party/
    └── doctest/
        └── doctest.h
```

`doctest` está incluido localmente para que la configuración no necesite Internet.

## Compilar y ejecutar

Desde esta carpeta:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

También puede ejecutar directamente todas las pruebas:

```bash
./build/utf8_tests
```

Para ejecutar solamente un ejercicio:

```bash
./build/utf8_tests --test-case="E04*"
```

La plantilla compila desde el inicio, pero las pruebas fallarán hasta completar las funciones.

## Ejercicios y puntuación

| Ejercicio | Descripción | Puntos |
|---:|---|---:|
| E01 | Lectura binaria del archivo | 8 |
| E02 | Identificación de bytes de continuación | 5 |
| E03 | Longitud según el byte líder | 8 |
| E04 | Decodificación de 2 bytes | 8 |
| E05 | Decodificación de 3 bytes | 8 |
| E06 | Decodificación de 4 bytes | 8 |
| E07 | Detección y omisión del BOM | 5 |
| E08 | Decodificación de un buffer válido | 18 |
| E09 | Detección de errores y recuperación | 20 |
| E10 | Representación y resumen | 12 |
| | **Total** | **100** |

## Contratos importantes

### `longitud_secuencia`

- Retorna `1`, `2`, `3` o `4` para los patrones líderes correspondientes.
- Retorna `0` para `10xxxxxx`.
- Retorna `-1` para cualquier otro patrón.

### Recuperación ante errores

- Una continuación que aparece sin líder genera `ContinuacionInesperada`.
- EOF o una continuación faltante genera `SecuenciaIncompleta` en el offset del líder.
- Otro patrón inválido genera `LiderInvalido`.
- Después del error se avanza exactamente un byte. Por ello, una secuencia truncada puede producir después otro error al procesar una continuación huérfana.

### Resumen

`bytes_totales` incluye todos los bytes leídos, incluido un BOM. El BOM no cuenta como code point. `por_longitud[0]` corresponde a secuencias de un byte y `por_longitud[3]` a secuencias de cuatro bytes.

## Consideraciones de almacenamiento

La lectura esperada es secuencial y de una sola pasada. Para un archivo de `n` bytes:

- CPU: `O(n)`.
- Memoria: `O(n)`, porque esta etapa almacena el archivo completo y los resultados.
- I/O: una lectura secuencial; no se necesitan búsquedas aleatorias ni llamadas a `seek`.
- No se debe interpretar el archivo como unidades de 16 o 32 bits: UTF-8 tiene longitud variable y se procesa byte por byte.
