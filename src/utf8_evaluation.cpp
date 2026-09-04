#include "utf8_evaluation.hpp"

#include <cstdint>
#include <format>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace utf8eval
{

std::vector<std::uint8_t> leer_archivo(const std::string &ruta)
{
    // E01 TODO: abra el archivo en modo binario, lea todos sus bytes y
    // lance std::runtime_error si no puede abrirlo o leerlo.

    std::ifstream in(ruta, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("No se puedo abrir el archivo.");

    auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0);

    if (!in)
        throw std::runtime_error("No se puedo leer el archivo.");

    std::vector<std::uint8_t> buffer(size);

    for (std::size_t i = 0; i < size; i++)
    {
        in.read(reinterpret_cast<char *>(&buffer[i]), sizeof(std::uint8_t));

        if (!in)
            throw std::runtime_error("No se pudo leer el archivo.");
    }

    return buffer;
}

bool es_continuacion(std::uint8_t byte) noexcept
{
    return ((byte & 0xC0) == 0x80);
}

int longitud_secuencia(std::uint8_t byte_lider) noexcept
{
    if (es_continuacion(byte_lider))
    {
        return 0;
    }

    // 0x80  0x00
    // 0xE0  0xC0
    // 0xF0  0xE0
    // 0xF8  0xF0

    if ((byte_lider & 0x80) == 0x00)
    {
        return 1; // one byte
    }

    if ((byte_lider & 0xE0) == 0xC0)
    {
        return 2; // two bytes
    }

    if ((byte_lider & 0xF0) == 0xE0)
    {
        return 3;
    }

    if ((byte_lider & 0xF8) == 0xF0)
    {
        return 4;
    }

    return -1;
}

std::uint32_t decodificar_2(std::uint8_t b1, std::uint8_t b2) noexcept
{
    // E04 TODO: extraiga y combine los bits utiles.

    if (longitud_secuencia(b1) != 2)
        return 0;

    if (!es_continuacion(b2))
        return 0;

    std::uint32_t codep = (((b1 & 0x1F) << 6) | (b2 & 0x3F));
    return codep;
}

std::uint32_t
decodificar_3(std::uint8_t b1, std::uint8_t b2, std::uint8_t b3) noexcept
{
    if (longitud_secuencia(b1) != 3)
        return 0;

    if (!es_continuacion(b2) || !es_continuacion(b3))
        return 0;

    std::uint32_t codep =
            (((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));

    return codep;
}

std::uint32_t decodificar_4(std::uint8_t b1,
                            std::uint8_t b2,
                            std::uint8_t b3,
                            std::uint8_t b4) noexcept
{
    if (longitud_secuencia(b1) != 4)
        return 0;

    if (!es_continuacion(b2) || !es_continuacion(b3) || !es_continuacion(b4))
        return 0;

    std::uint32_t codep = (((b1 & 0x07) << 18) | ((b2 & 0x3F) << 12) |
                           ((b3 & 0x3F) << 6) | (b4 & 0x3F));

    return codep;
}

std::size_t offset_inicial(const std::vector<std::uint8_t> &bytes) noexcept
{
    // E07 TODO: omita EF BB BF solamente cuando sea el prefijo del archivo.
    if (bytes.size() < 3)
        return 0;

    if ((bytes[0] == 0xEF) && (bytes[1] == 0xBB) && (bytes[2] == 0xBF))
        return 3;
    return 0;
}

ResultadoDecodificacion decodificar(const std::vector<std::uint8_t> &bytes)
{
    // E08 TODO: decodifique completamente un buffer UTF-8 valido y almacene
    // cada code point con su offset y cantidad original de bytes.
    //
    // E09 TODO: detecte errores, almacenelos y recupere el procesamiento
    // avanzando un solo byte desde el byte lider/problematico.

    std::vector<UnidadDecodificada> unidades;
    std::vector<ErrorDecodificacion> errores;

    auto offset = static_cast<int>(offset_inicial(bytes));

    while (offset < static_cast<int>(bytes.size()))
    {
        auto b1 = bytes[offset];

        auto length = longitud_secuencia(b1);

        if (length == -1)
        {
            errores.emplace_back(TipoError::LiderInvalido, offset);
            offset++;
            continue;
        }

        if (length == 0)
        {
            errores.emplace_back(TipoError::ContinuacionInesperada, offset);
            offset++;
            continue;
        }

        if (length == 1)
        {
            unidades.emplace_back(b1, offset, length);
            offset++;
        }
        else if (length == 2)
        {
            if ((offset + 1) >= static_cast<int>(bytes.size()))
            {
                errores.emplace_back(TipoError::SecuenciaIncompleta, offset);
                offset++;
                continue;
            }

            auto b2 = bytes[offset + 1];
            if (!es_continuacion(b2))
            {
                errores.emplace_back(TipoError::SecuenciaIncompleta, offset);
                offset++;
                continue;
            }

            auto codep = decodificar_2(b1, b2);
            unidades.emplace_back(codep, offset, length);
            offset += length;
        }
        else if (length == 3)
        {
            if ((offset + 2) >= static_cast<int>(bytes.size()))
            {
                errores.emplace_back(TipoError::SecuenciaIncompleta, offset);
                offset++;
                continue;
            }

            auto b2 = bytes[offset + 1];
            auto b3 = bytes[offset + 2];

            if (!es_continuacion(b2) || !es_continuacion(b3))
            {
                errores.emplace_back(TipoError::SecuenciaIncompleta, offset);
                offset++;
                continue;
            }

            auto codep = decodificar_3(b1, b2, b3);
            unidades.emplace_back(codep, offset, length);
            offset += length;
        }
        else if (length == 4)
        {
            if ((offset + 3) >= static_cast<int>(bytes.size()))
            {
                errores.emplace_back(TipoError::SecuenciaIncompleta, offset);
                offset++;
                continue;
            }

            auto b2 = bytes[offset + 1];
            auto b3 = bytes[offset + 2];
            auto b4 = bytes[offset + 3];

            if (!es_continuacion(b2) || !es_continuacion(b3) ||
                !es_continuacion(b4))
            {
                errores.emplace_back(TipoError::SecuenciaIncompleta, offset);
                offset++;
                continue;
            }

            auto codep = decodificar_4(b1, b2, b3, b4);
            unidades.emplace_back(codep, offset, length);
            offset += length;
        }
    }

    return {unidades, errores};
}

std::string representar_codepoint(std::uint32_t codepoint)
{
    // E10 TODO: ASCII imprimible (0x20..0x7E) se representa como caracter.
    // Todo lo demas debe usar std::format con el formato U+XXXX.
    if ((codepoint >= 0x20) && (codepoint <= 0x7E))
    {
        return std::format("{}", static_cast<char>(codepoint));
    }

    return std::format("U+{:04X}", codepoint);
}

Resumen calcular_resumen(std::size_t bytes_totales,
                         const ResultadoDecodificacion &resultado) noexcept
{
    // E10 TODO: calcule los totales sin volver a decodificar los bytes.

    auto codepoints = resultado.unidades;
    auto errores = resultado.errores;

    std::size_t oneb = 0, twob = 0, threeb = 0, fourb = 0;

    for (const auto &codep : codepoints)
    {
        switch (codep.cantidad_bytes)
        {
        case 1:
            oneb++;
            break;
        case 2:
            twob++;
            break;
        case 3:
            threeb++;
            break;
        case 4:
            fourb++;
            break;
        default:
            break;
        }
    }

    return Resumen{bytes_totales,
                   codepoints.size(),
                   {oneb, twob, threeb, fourb},
                   errores.size()};
}

} // namespace utf8eval
