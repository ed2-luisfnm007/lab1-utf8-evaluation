cmake_minimum_required(VERSION 3.20)

project(UTF8Evaluation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(utf8_evaluation
    src/utf8_evaluation.cpp
)

target_include_directories(utf8_evaluation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(doctest_headers INTERFACE)
target_include_directories(doctest_headers
    SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

add_executable(utf8_tests
    tests/tests.cpp
)

target_link_libraries(utf8_tests
    PRIVATE
        utf8_evaluation
        doctest_headers
)

if(MSVC)
    target_compile_options(utf8_evaluation PRIVATE /W4)
    target_compile_options(utf8_tests PRIVATE /W4)
else()
    target_compile_options(utf8_evaluation PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(utf8_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    function(agregar_ejercicio etiqueta puntos)
        add_test(
            NAME ejercicio_${etiqueta}
            COMMAND utf8_tests --test-case=E${etiqueta}*
        )
        set_tests_properties(
            ejercicio_${etiqueta}
            PROPERTIES LABELS "${puntos}_puntos"
        )
    endfunction()

    agregar_ejercicio(01 8)
    agregar_ejercicio(02 5)
    agregar_ejercicio(03 8)
    agregar_ejercicio(04 8)
    agregar_ejercicio(05 8)
    agregar_ejercicio(06 8)
    agregar_ejercicio(07 5)
    agregar_ejercicio(08 18)
    agregar_ejercicio(09 20)
    agregar_ejercicio(10 12)
endif()
