cmake_minimum_required(VERSION 3.27)
project(picoputt VERSION 0.1.0
    LANGUAGES C
    DESCRIPTION "Quantum miniature golf game"
    HOMEPAGE_URL "https://github.com/benburrill/picoputt"
)

set(CMAKE_C_STANDARD 11)

file(GLOB PICOPUTT_SRC src/*.c)
add_executable(picoputt ${PICOPUTT_SRC})


find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(picoputt
    PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    SDL2::SDL2-static
)

find_package(glew CONFIG REQUIRED)
target_link_libraries(picoputt PRIVATE GLEW::glew_s)

if (WIN32)
    configure_file(README_release_build.txt.in README.txt NEWLINE_STYLE CRLF)
else ()
    configure_file(README_release_build.txt.in README.txt NEWLINE_STYLE LF)
endif ()

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
install(TARGETS picoputt RUNTIME DESTINATION .)
install(DIRECTORY shaders DESTINATION .)
install(DIRECTORY images DESTINATION .)
install(FILES COPYING DESTINATION .)
install(FILES ${CMAKE_BINARY_DIR}/README.txt DESTINATION .)
include(CPack)
