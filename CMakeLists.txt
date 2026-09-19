cmake_minimum_required(VERSION 3.16)
project(bens-dm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

find_package(X11 REQUIRED)

add_executable(bens-dm
    src/main.cpp
    src/wm/window_manager.cpp
    src/layouts/tiling_layout.cpp
    src/config/config.cpp
)

target_include_directories(bens-dm PRIVATE include)
target_link_libraries(bens-dm PRIVATE ${X11_LIBRARIES})
target_include_directories(bens-dm PRIVATE ${X11_INCLUDE_DIR})

install(TARGETS bens-dm RUNTIME DESTINATION bin)

# Session launcher: hiermee verschijnt "Bens DM" als kiesbare sessie in
# display managers (LightDM, SDDM, ...). Dit is de vertaling van de
# BENS OS INTEGRATIE-eis dat de binary op /usr/bin/bens-dm terechtkomt én
# als échte desktopsessie startbaar is, niet alleen als los programma.
install(FILES assets/bens-dm.desktop DESTINATION share/xsessions)

# Layout-logica is pure berekening (geen X-server nodig), dus die testen we
# los van de rest met een simpel testbinary.
enable_testing()
add_executable(layout_test
    tests/layout_test.cpp
    src/layouts/tiling_layout.cpp
)
target_include_directories(layout_test PRIVATE include)
target_include_directories(layout_test PRIVATE ${X11_INCLUDE_DIR})
add_test(NAME layout_test COMMAND layout_test)
