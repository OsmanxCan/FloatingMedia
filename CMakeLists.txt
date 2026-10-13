cmake_minimum_required(VERSION 3.21)

project(FloatingMedia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FLOATINGMEDIA_INSTALL_KWIN_SCRIPT
    "Install the KDE Plasma KWin keep-above script (optional desktop integration)"
    ON
)
option(FLOATINGMEDIA_BUILD_TESTS "Build unit/smoke tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Quick
    QuickControls2
    Multimedia
    Network
    DBus
)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(FloatingMediaApp
    src/main.cpp
    src/AppBridge.cpp
    src/AppBridge.h
    src/NativeMessagingHost.cpp
    src/NativeMessagingHost.h
    src/MediaUrl.h
    src/KWinKeepAbove.cpp
    src/KWinKeepAbove.h
)

qt_add_qml_module(FloatingMediaApp
    URI FloatingMedia
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(FloatingMediaApp
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia
        Qt6::Network
        Qt6::DBus
)

include(GNUInstallDirs)

install(TARGETS FloatingMediaApp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/floatingmedia.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/floatingmedia.desktop
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/floatingmedia.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    COMPONENT Runtime
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/floatingmedia.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    COMPONENT Runtime
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/browser-extension
    DESTINATION ${CMAKE_INSTALL_DATADIR}/floatingmedia
    COMPONENT Browser
)

if(FLOATINGMEDIA_INSTALL_KWIN_SCRIPT)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/kwin/scripts/floatingmedia-keep-above
        COMPONENT KDE
    )
endif()

if(FLOATINGMEDIA_BUILD_TESTS)
    enable_testing()
    add_executable(test_media_url tests/test_media_url.cpp)
    target_include_directories(test_media_url PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_media_url PRIVATE Qt6::Core)
    add_test(NAME media_url COMMAND test_media_url)

    add_test(
        NAME install_dry_run
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_script_checks.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
