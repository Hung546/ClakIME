cmake_minimum_required(VERSION 3.16)
project(clak VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM 1.0.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

string(REPLACE "-fno-operator-names" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

find_package(Fcitx5Core 5.0.14 REQUIRED)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

set(CLAK_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/engine")
set(CLAK_ENGINE_LIB "${CMAKE_CURRENT_BINARY_DIR}/engine-target/release/libclak_engine.a")

file(GLOB_RECURSE CLAK_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CLAK_ENGINE_DIR}/src/*.rs"
    "${CLAK_ENGINE_DIR}/data/*"
    "${CLAK_ENGINE_DIR}/Cargo.toml"
)

add_custom_command(
    OUTPUT ${CLAK_ENGINE_LIB}
    COMMAND cargo build --release
        --manifest-path "${CLAK_ENGINE_DIR}/Cargo.toml"
        --target-dir "${CMAKE_CURRENT_BINARY_DIR}/engine-target"
    WORKING_DIRECTORY ${CLAK_ENGINE_DIR}
    DEPENDS ${CLAK_ENGINE_SOURCES}
    VERBATIM
)
add_custom_target(clak_engine_build DEPENDS ${CLAK_ENGINE_LIB})

set(CLAK_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(CLAK_UI_BIN "${CMAKE_CURRENT_BINARY_DIR}/ui-target/release/clak-gui")

file(GLOB_RECURSE CLAK_UI_SOURCES CONFIGURE_DEPENDS
    "${CLAK_UI_DIR}/src/*"
    "${CLAK_UI_DIR}/Cargo.toml"
    "${CLAK_UI_DIR}/build.rs"
)

add_custom_command(
    OUTPUT ${CLAK_UI_BIN}
    COMMAND cargo build --release
        --manifest-path "${CLAK_UI_DIR}/Cargo.toml"
        --target-dir "${CMAKE_CURRENT_BINARY_DIR}/ui-target"
    WORKING_DIRECTORY ${CLAK_UI_DIR}
    DEPENDS ${CLAK_UI_SOURCES} ${CLAK_ENGINE_SOURCES}
    VERBATIM
)
add_custom_target(clak_gui_build ALL DEPENDS ${CLAK_UI_BIN})

add_library(clak_rust_engine INTERFACE)
target_link_libraries(clak_rust_engine INTERFACE "${CLAK_ENGINE_LIB}")
add_dependencies(clak_rust_engine clak_engine_build)

set(CLAK_SOURCES
    src/config/sites.cpp
    src/platform/hyprland.cpp
    src/platform/modal_editor.cpp
    src/uinput/uinput.cpp
    src/utils/log.cpp
    src/utils/text_utils.cpp
    src/ime/state.cpp
    src/engine.cpp
)

add_library(clak MODULE ${CLAK_SOURCES})
set_target_properties(clak PROPERTIES
    OUTPUT_NAME "clak"
    PREFIX "lib"
    POSITION_INDEPENDENT_CODE ON
)
target_link_options(clak PRIVATE "-Wl,-exclude-libs,ALL")

target_include_directories(clak PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
)

target_link_libraries(clak PRIVATE
    Fcitx5::Core
    clak_rust_engine
    pthread
    dl
    m
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/clak-addon.conf.in"
    "${CMAKE_CURRENT_BINARY_DIR}/clak.conf"
    COPYONLY
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/clak-local.conf"
    CONTENT "[Addon]\nName=Clak\nName[vi]=Clak\nComment=Fast Vietnamese input method\nCategory=InputMethod\nVersion=0.1.0\nEnabled=True\nIcon=org.fcitx.Fcitx5.clak\nLibrary=${CMAKE_CURRENT_BINARY_DIR}/libclak\nType=SharedLibrary\nOnDemand=True\nConfigurable=False\n[Dependencies]\n0=core:5.0.14\n"
)

install(TARGETS clak DESTINATION "${FCITX_INSTALL_ADDONDIR}")
install(PROGRAMS "${CLAK_UI_BIN}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/clak-gui.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/clak.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/clak.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/" DESTINATION "${CMAKE_INSTALL_DATADIR}/icons")

add_custom_target(install-user
    COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/bin"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/share/applications"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/share/fcitx5/addon"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/share/fcitx5/inputmethod"
    COMMAND ${CMAKE_COMMAND} -E copy "${CLAK_UI_BIN}" "$ENV{HOME}/.local/bin/clak-gui"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/data/clak-gui.desktop" "$ENV{HOME}/.local/share/applications/clak-gui.desktop"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/clak-local.conf" "$ENV{HOME}/.local/share/fcitx5/addon/clak.conf"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/data/clak.conf" "$ENV{HOME}/.local/share/fcitx5/inputmethod/clak.conf"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/hicolor" "$ENV{HOME}/.local/share/icons/hicolor"
    COMMAND gtk-update-icon-cache -f -t "$ENV{HOME}/.local/share/icons/hicolor" || true
    DEPENDS clak clak_gui_build
)
