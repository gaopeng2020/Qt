cmake_minimum_required(VERSION 3.16)
project(TcpFileTransfer VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(TcpFileTransfer WIN32 MACOSX_BUNDLE
    main.cpp
    tcpfiletransferclient.cpp tcpfiletransferclient.h tcpfiletransferclient.ui
    tcpfiletransferserver.cpp tcpfiletransferserver.h tcpfiletransferserver.ui
)
target_link_libraries(TcpFileTransfer PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
)

if((QT_VERSION_MAJOR GREATER 4))
    target_link_libraries(TcpFileTransfer PRIVATE
        Qt::Widgets
    )
endif()

install(TARGETS TcpFileTransfer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET TcpFileTransfer
    FILENAME_VARIABLE deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
