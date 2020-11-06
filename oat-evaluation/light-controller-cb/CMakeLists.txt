cmake_minimum_required(VERSION 3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99 -Wall -Wextra -Werror")

add_executable(light-controller
    light-controller.c)

install(TARGETS light-controller RUNTIME DESTINATION bin)
install(PROGRAMS light-controller-build DESTINATION bin)

target_link_libraries(light-controller
    telldus-core)
