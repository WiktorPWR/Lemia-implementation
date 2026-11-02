cmake_minimum_required(VERSION 3.24)

# Architektura GPU: GTX 1050 = Pascal = 61. Musi byc ustawiona PRZED project().
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 61)
endif()

project(CPU_and_GPU LANGUAGES CXX CUDA)

# compile_commands.json dla clangd (podpowiadanie w VS Code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Domyslnie Release (szybki kod), chyba ze podano inaczej
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CUDAToolkit REQUIRED)

# ---------- Pliki zrodlowe ----------
# Wszystkie .cpp i .cu z folderu scr/ (tez z podfolderow) sa dodawane automatycznie.
# CONFIGURE_DEPENDS = nowy plik zostanie wykryty przy kolejnym budowaniu.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scr/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scr/*.cu"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ---------- Headery ----------
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

# ---------- Biblioteki ----------
target_link_libraries(${PROJECT_NAME} PRIVATE
    CUDA::cudart
    # CUDA::cufft     # FFT na GPU (przyda sie do splotu w Lenii)
    # CUDA::cublas    # algebra liniowa na GPU
)

# ---------- Flagi kompilatora ----------
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4 /utf-8>
    $<$<COMPILE_LANGUAGE:CUDA>:-Wno-deprecated-gpu-targets -Xcompiler=/utf-8>
)
