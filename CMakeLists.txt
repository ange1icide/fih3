cmake_minimum_required(VERSION 3.20)
project(fih3)

set(CMAKE_CXX_STANDARD 20)

add_executable(fih3
        src/main.cpp
        src/board.cpp
        src/movegen.cpp
        src/eval.cpp
        src/tt.cpp
        src/search.cpp
        src/uci.cpp
        src/bitboard.cpp
        src/include/bitboard.h
)

target_include_directories(fih3 PRIVATE src/include)

target_compile_options(fih3 PRIVATE
        $<$<CONFIG:Release>:
        -O3 -march=native -mtune=native -flto
        -DNDEBUG -funroll-loops -fno-exceptions
        -fomit-frame-pointer
        >
        $<$<CONFIG:Debug>:
        -O0 -g3 -fsanitize=address,undefined
        >
)

target_link_options(fih3 PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)