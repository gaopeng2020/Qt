cmake_minimum_required(VERSION 3.16)
project(TCP_Communication_Test VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(TCP_Communication_Test WIN32 MACOSX_BUNDLE
    main.cpp
    tcpclient.cpp tcpclient.h tcpclient.ui
    tcpserver.cpp tcpserver.h tcpserver.ui
)
target_link_libraries(TCP_Communication_Test PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
)

if((QT_VERSION_MAJOR GREATER 4))
    target_link_libraries(TCP_Communication_Test PRIVATE
        Qt::Widgets
    )
endif()

install(TARGETS TCP_Communication_Test
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET TCP_Communication_Test
    FILENAME_VARIABLE deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
