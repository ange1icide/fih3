cmake_minimum_required(VERSION 3.20)
project(fih3)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_RELEASE
        "-O3 -march=native -mtune=native -flto -DNDEBUG \
     -funroll-loops -fno-exceptions -fomit-frame-pointer"
)

add_executable(fih3 main.cpp)

target_compile_options(fih3 PRIVATE
        $<$<CONFIG:Release>:
        -O3
        -march=native
        -mtune=native
        -flto
        -DNDEBUG
        -funroll-loops
        -fno-exceptions
        -fomit-frame-pointer
        >
        $<$<CONFIG:Debug>:
        -O0
        -g3
        -fsanitize=address,undefined
        >
)

target_link_options(fih3 PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)