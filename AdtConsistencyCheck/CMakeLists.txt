cmake_minimum_required(VERSION 3.16)

#  vcpkg 的 triplet 设置（根据您的工具链选择），必须放在project之前
#  常用值：x64-windows (MSVC)、x64-mingw-dynamic (MinGW动态)、x64-mingw-static (MinGW静态)
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg triplet")

project(AdtConsistencyCheck VERSION 1.0.0 LANGUAGES CXX)

#ui不在根目录时必须告诉cmake ui的路径
set(CMAKE_AUTOUIC_SEARCH_PATHS "ui")
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        src/AdtConsistencyCheckApp.cpp
        src/DataTypeConsistencyCheck.cpp
        #h,cpp以及ui分类存放时，必须添加头文件以及ui，否则找不到ui_xxx.h
        include/AdtConsistencyCheckApp.h
        include/DataTypeConsistencyCheck.h
        ui/AdtConsistencyCheckApp.ui
)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
            resource.qrc
            resource.rc
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET AdtConsistencyCheck APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
    if (ANDROID)
        add_library(${PROJECT_NAME} SHARED
                ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_executable(${PROJECT_NAME} ${PROJECT_SOURCES}
        )
    endif ()
endif ()

# h,cpp以及ui分类存放时才需要
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

#find_package(CommonUtilsLib REQUIRED)
#find_package(glog CONFIG REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE CommonUtilsLib::CommonUtilsLib glog::glog)

find_package(common-utils CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE common-utils::common-utils)

# 构建时复制conf文件到CMAKE_BINARY_DIR
add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/conf
        ${CMAKE_BINARY_DIR}/conf
        COMMENT "Copying config directory to build folder"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if (${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
endif ()
set_target_properties(${PROJECT_NAME} PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif ()
