#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "utf8_evaluation.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

using utf8eval::ErrorDecodificacion;
using utf8eval::ResultadoDecodificacion;
using utf8eval::TipoError;
using utf8eval::UnidadDecodificada;

class ArchivoTemporal {
public:
    explicit ArchivoTemporal(const std::vector<std::uint8_t>& contenido) {
        static std::atomic<unsigned long long> contador{0};
        const auto instante = std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count();
        ruta_ = std::filesystem::temp_directory_path() /
                ("utf8_evaluation_" + std::to_string(instante) + "_" +
                 std::to_string(contador++) + ".bin");

        std::ofstream salida(ruta_, std::ios::binary | std::ios::trunc);
        if (!salida) {
            throw std::runtime_error("No se pudo crear el archivo temporal");
        }

        if (!contenido.empty()) {
            salida.write(
                reinterpret_cast<const char*>(contenido.data()),
                static_cast<std::streamsize>(contenido.size()));
        }

        if (!salida) {
            throw std::runtime_error("No se pudo escribir el archivo temporal");
        }
    }

    ArchivoTemporal(const ArchivoTemporal&) = delete;
    ArchivoTemporal& operator=(const ArchivoTemporal&) = delete;

    ~ArchivoTemporal() {
        std::error_code error;
        std::filesystem::remove(ruta_, error);
    }

    [[nodiscard]] std::string ruta() const {
        return ruta_.string();
    }

private:
    std::filesystem::path ruta_;
};

void verificar_unidad(
    const UnidadDecodificada& unidad,
    std::uint32_t codepoint,
    std::size_t offset,
    std::uint8_t cantidad_bytes) {
    CHECK(unidad.codepoint == codepoint);
    CHECK(unidad.offset == offset);
    CHECK(unidad.cantidad_bytes == cantidad_bytes);
}

void verificar_error(
    const ErrorDecodificacion& error,
    TipoError tipo,
    std::size_t offset) {
    CHECK(error.tipo == tipo);
    CHECK(error.offset == offset);
}

}  // namespace

TEST_CASE("E01 - lectura binaria del archivo") {
    using utf8eval::leer_archivo;

    SUBCASE("preserva todos los valores de byte") {
        const std::vector<std::uint8_t> esperado{
            0x41, 0x00, 0x0A, 0x1A, 0xC3, 0xA9, 0xFF};
        const ArchivoTemporal archivo(esperado);

        CHECK(leer_archivo(archivo.ruta()) == esperado);
    }

    SUBCASE("un archivo vacio produce un vector vacio") {
        const ArchivoTemporal archivo({});
        CHECK(leer_archivo(archivo.ruta()).empty());
    }

    SUBCASE("una ruta inexistente genera una excepcion") {
        const auto ruta = std::filesystem::temp_directory_path() /
                          "utf8_evaluation_archivo_que_no_existe.bin";
        std::error_code error;
        std::filesystem::remove(ruta, error);

        CHECK_THROWS_AS(leer_archivo(ruta.string()), std::runtime_error);
    }
}

TEST_CASE("E02 - identificacion de bytes de continuacion") {
    using utf8eval::es_continuacion;

    CHECK_FALSE(es_continuacion(0x00));
    CHECK_FALSE(es_continuacion(0x7F));
    CHECK(es_continuacion(0x80));
    CHECK(es_continuacion(0x91));
    CHECK(es_continuacion(0xBF));
    CHECK_FALSE(es_continuacion(0xC0));
    CHECK_FALSE(es_continuacion(0xFF));
}

TEST_CASE("E03 - longitud indicada por el byte lider") {
    using utf8eval::longitud_secuencia;

    SUBCASE("secuencias de un byte") {
        CHECK(longitud_secuencia(0x00) == 1);
        CHECK(longitud_secuencia(0x41) == 1);
        CHECK(longitud_secuencia(0x7F) == 1);
    }

    SUBCASE("secuencias de dos bytes") {
        CHECK(longitud_secuencia(0xC0) == 2);
        CHECK(longitud_secuencia(0xC3) == 2);
        CHECK(longitud_secuencia(0xDF) == 2);
    }

    SUBCASE("secuencias de tres bytes") {
        CHECK(longitud_secuencia(0xE0) == 3);
        CHECK(longitud_secuencia(0xE2) == 3);
        CHECK(longitud_secuencia(0xEF) == 3);
    }

    SUBCASE("secuencias de cuatro bytes") {
        CHECK(longitud_secuencia(0xF0) == 4);
        CHECK(longitud_secuencia(0xF4) == 4);
        CHECK(longitud_secuencia(0xF7) == 4);
    }

    SUBCASE("continuaciones y otros patrones") {
        CHECK(longitud_secuencia(0x80) == 0);
        CHECK(longitud_secuencia(0xBF) == 0);
        CHECK(longitud_secuencia(0xF8) == -1);
        CHECK(longitud_secuencia(0xFF) == -1);
    }
}

TEST_CASE("E04 - decodificacion de secuencias de dos bytes") {
    using utf8eval::decodificar_2;

    CHECK(decodificar_2(0xC2, 0x80) == 0x0080);
    CHECK(decodificar_2(0xC3, 0xA9) == 0x00E9);
    CHECK(decodificar_2(0xD1, 0x91) == 0x0451);
    CHECK(decodificar_2(0xDF, 0xBF) == 0x07FF);
}

TEST_CASE("E05 - decodificacion de secuencias de tres bytes") {
    using utf8eval::decodificar_3;

    CHECK(decodificar_3(0xE0, 0xA0, 0x80) == 0x0800);
    CHECK(decodificar_3(0xE2, 0x82, 0xAC) == 0x20AC);
    CHECK(decodificar_3(0xE6, 0xBC, 0xA2) == 0x6F22);
    CHECK(decodificar_3(0xEF, 0xBF, 0xBF) == 0xFFFF);
}

TEST_CASE("E06 - decodificacion de secuencias de cuatro bytes") {
    using utf8eval::decodificar_4;

    CHECK(decodificar_4(0xF0, 0x90, 0x80, 0x80) == 0x10000);
    CHECK(decodificar_4(0xF0, 0x90, 0x8D, 0x88) == 0x10348);
    CHECK(decodificar_4(0xF0, 0x9F, 0x98, 0x80) == 0x1F600);
    CHECK(decodificar_4(0xF4, 0x8F, 0xBF, 0xBF) == 0x10FFFF);
}

TEST_CASE("E07 - deteccion del BOM al inicio") {
    using utf8eval::offset_inicial;

    CHECK(offset_inicial({}) == 0);
    CHECK(offset_inicial({0xEF}) == 0);
    CHECK(offset_inicial({0xEF, 0xBB}) == 0);
    CHECK(offset_inicial({0xEF, 0xBB, 0xBF}) == 3);
    CHECK(offset_inicial({0xEF, 0xBB, 0xBF, 0x41}) == 3);
    CHECK(offset_inicial({0x41, 0xEF, 0xBB, 0xBF}) == 0);
    CHECK(offset_inicial({0xEF, 0xBB, 0xBE}) == 0);
}

TEST_CASE("E08 - decodificacion de un buffer UTF-8 valido") {
    using utf8eval::decodificar;

    SUBCASE("buffer vacio") {
        const auto resultado = decodificar({});
        CHECK(resultado.unidades.empty());
        CHECK(resultado.errores.empty());
    }

    SUBCASE("mezcla secuencias de una a cuatro bytes") {
        const std::vector<std::uint8_t> bytes{
            0x41,
            0xC3, 0xA9,
            0xE2, 0x82, 0xAC,
            0xF0, 0x9F, 0x98, 0x80};

        const auto resultado = decodificar(bytes);

        REQUIRE(resultado.errores.empty());
        REQUIRE(resultado.unidades.size() == 4);
        verificar_unidad(resultado.unidades[0], 0x0041, 0, 1);
        verificar_unidad(resultado.unidades[1], 0x00E9, 1, 2);
        verificar_unidad(resultado.unidades[2], 0x20AC, 3, 3);
        verificar_unidad(resultado.unidades[3], 0x1F600, 6, 4);
    }

    SUBCASE("omite el BOM pero conserva offsets absolutos") {
        const auto resultado = decodificar({0xEF, 0xBB, 0xBF, 0x41, 0xC3, 0xA9});

        REQUIRE(resultado.errores.empty());
        REQUIRE(resultado.unidades.size() == 2);
        verificar_unidad(resultado.unidades[0], 0x0041, 3, 1);
        verificar_unidad(resultado.unidades[1], 0x00E9, 4, 2);
    }
}

TEST_CASE("E09 - deteccion de errores y recuperacion") {
    using utf8eval::decodificar;

    SUBCASE("continuacion inesperada seguida de ASCII valido") {
        const auto resultado = decodificar({0x80, 0x41});

        REQUIRE(resultado.errores.size() == 1);
        verificar_error(resultado.errores[0], TipoError::ContinuacionInesperada, 0);
        REQUIRE(resultado.unidades.size() == 1);
        verificar_unidad(resultado.unidades[0], 0x0041, 1, 1);
    }

    SUBCASE("continuacion faltante y recuperacion de los bytes siguientes") {
        const auto resultado = decodificar({0xE2, 0x41, 0x42});

        REQUIRE(resultado.errores.size() == 1);
        verificar_error(resultado.errores[0], TipoError::SecuenciaIncompleta, 0);
        REQUIRE(resultado.unidades.size() == 2);
        verificar_unidad(resultado.unidades[0], 0x0041, 1, 1);
        verificar_unidad(resultado.unidades[1], 0x0042, 2, 1);
    }

    SUBCASE("secuencia truncada al final sin acceso fuera del vector") {
        const auto resultado = decodificar({0xF0, 0x9F});

        REQUIRE(resultado.unidades.empty());
        REQUIRE(resultado.errores.size() == 2);
        verificar_error(resultado.errores[0], TipoError::SecuenciaIncompleta, 0);
        verificar_error(resultado.errores[1], TipoError::ContinuacionInesperada, 1);
    }

    SUBCASE("un lider invalido no descarta el siguiente caracter") {
        const auto resultado = decodificar({0xF8, 0x5A});

        REQUIRE(resultado.errores.size() == 1);
        verificar_error(resultado.errores[0], TipoError::LiderInvalido, 0);
        REQUIRE(resultado.unidades.size() == 1);
        verificar_unidad(resultado.unidades[0], 0x005A, 1, 1);
    }

    SUBCASE("el byte que rompio una secuencia puede iniciar otra valida") {
        const auto resultado = decodificar({0xC3, 0xC3, 0xA9});

        REQUIRE(resultado.errores.size() == 1);
        verificar_error(resultado.errores[0], TipoError::SecuenciaIncompleta, 0);
        REQUIRE(resultado.unidades.size() == 1);
        verificar_unidad(resultado.unidades[0], 0x00E9, 1, 2);
    }
}

TEST_CASE("E10 - representacion de code points y resumen") {
    using utf8eval::calcular_resumen;
    using utf8eval::representar_codepoint;

    SUBCASE("ASCII imprimible se muestra como caracter") {
        CHECK(representar_codepoint(0x20) == " ");
        CHECK(representar_codepoint(0x41) == "A");
        CHECK(representar_codepoint(0x7E) == "~");
    }

    SUBCASE("los demas code points se muestran como U+XXXX") {
        CHECK(representar_codepoint(0x00) == "U+0000");
        CHECK(representar_codepoint(0x0A) == "U+000A");
        CHECK(representar_codepoint(0x7F) == "U+007F");
        CHECK(representar_codepoint(0x00E9) == "U+00E9");
        CHECK(representar_codepoint(0x20AC) == "U+20AC");
        CHECK(representar_codepoint(0x1F600) == "U+1F600");
    }

    SUBCASE("calcula un resumen a partir de resultados almacenados") {
        ResultadoDecodificacion resultado;
        resultado.unidades = {
            {0x0041, 3, 1},
            {0x00E9, 4, 2},
            {0x20AC, 6, 3},
            {0x1F600, 9, 4}};
        resultado.errores = {
            {TipoError::ContinuacionInesperada, 13},
            {TipoError::SecuenciaIncompleta, 14}};

        const auto resumen = calcular_resumen(15, resultado);

        CHECK(resumen.bytes_totales == 15);
        CHECK(resumen.codepoints_validos == 4);
        CHECK(resumen.por_longitud == std::array<std::size_t, 4>{1, 1, 1, 1});
        CHECK(resumen.errores == 2);
    }

    SUBCASE("resume correctamente un resultado vacio") {
        const auto resumen = calcular_resumen(0, {});

        CHECK(resumen.bytes_totales == 0);
        CHECK(resumen.codepoints_validos == 0);
        CHECK(resumen.por_longitud == std::array<std::size_t, 4>{0, 0, 0, 0});
        CHECK(resumen.errores == 0);
    }
}
