#ifndef UTF8_EVALUATION_HPP
#define UTF8_EVALUATION_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace utf8eval {

enum class TipoError {
    ContinuacionInesperada,
    SecuenciaIncompleta,
    LiderInvalido
};

struct UnidadDecodificada {
    std::uint32_t codepoint{};
    std::size_t offset{};
    std::uint8_t cantidad_bytes{};

    bool operator==(const UnidadDecodificada&) const = default;
};

struct ErrorDecodificacion {
    TipoError tipo{};
    std::size_t offset{};

    bool operator==(const ErrorDecodificacion&) const = default;
};

struct ResultadoDecodificacion {
    std::vector<UnidadDecodificada> unidades;
    std::vector<ErrorDecodificacion> errores;
};

struct Resumen {
    std::size_t bytes_totales{};
    std::size_t codepoints_validos{};
    // Posiciones 0, 1, 2 y 3: cantidades codificadas originalmente
    // con 1, 2, 3 y 4 bytes, respectivamente.
    std::array<std::size_t, 4> por_longitud{};
    std::size_t errores{};

    bool operator==(const Resumen&) const = default;
};

// E01 (8 puntos)
std::vector<std::uint8_t> leer_archivo(const std::string& ruta);

// E02 (5 puntos)
bool es_continuacion(std::uint8_t byte) noexcept;

// E03 (8 puntos)
// Retorna 1..4 para un byte lider, 0 para un byte de continuacion
// y -1 para cualquier otro patron.
int longitud_secuencia(std::uint8_t byte_lider) noexcept;

// E04-E06 (8 puntos cada uno).
// Estas funciones reciben secuencias cuya estructura ya fue validada.
std::uint32_t decodificar_2(std::uint8_t b1, std::uint8_t b2) noexcept;
std::uint32_t decodificar_3(
    std::uint8_t b1,
    std::uint8_t b2,
    std::uint8_t b3) noexcept;
std::uint32_t decodificar_4(
    std::uint8_t b1,
    std::uint8_t b2,
    std::uint8_t b3,
    std::uint8_t b4) noexcept;

// E07 (5 puntos): retorna 3 si EF BB BF aparece al inicio; de lo contrario, 0.
std::size_t offset_inicial(const std::vector<std::uint8_t>& bytes) noexcept;

// E08-E09 (18 y 20 puntos).
// Debe almacenar resultados y errores; no debe imprimir durante la decodificacion.
ResultadoDecodificacion decodificar(const std::vector<std::uint8_t>& bytes);

// E10 (12 puntos entre ambas funciones).
std::string representar_codepoint(std::uint32_t codepoint);
Resumen calcular_resumen(
    std::size_t bytes_totales,
    const ResultadoDecodificacion& resultado) noexcept;

}  // namespace utf8eval

#endif  // UTF8_EVALUATION_HPP
