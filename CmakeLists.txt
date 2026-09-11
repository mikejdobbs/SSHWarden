cmake_minimum_required(VERSION 3.10)
project(SSHSlapper C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(SSHSlapper main.c)

target_compile_options(SSHSlapper PRIVATE
    -Wall
    -Wextra
    -Werror
    -pedantic
)

target_compile_definitions(SSHSlapper PRIVATE
    _POSIX_C_SOURCE=200809L
)

install(TARGETS SSHSlapper
    RUNTIME DESTINATION bin
)
