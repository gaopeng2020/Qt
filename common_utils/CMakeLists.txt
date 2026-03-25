cmake_minimum_required(VERSION 3.15)
# ===================================== 用户配置区 =====================================
# 使用前请修改以下变量

# 1. 库的名称前缀（例如 Calculator -> CalculatorLib）
set(LIB_PREFIX CommonUtils)

# 2. 库类型：SHARED（动态库）或 STATIC（静态库）
set(LIB_TYPE SHARED CACHE STRING "库类型（SHARED动态库/STATIC静态库）")
if (NOT LIB_TYPE STREQUAL "SHARED" AND NOT LIB_TYPE STREQUAL "STATIC")
    message(FATAL_ERROR "LIB_TYPE must be SHARED or STATIC")
endif ()

# 3. vcpkg 的 triplet 设置（根据您的工具链选择）
#    常用值：x64-windows (MSVC)、x64-mingw-dynamic (MinGW动态)、x64-mingw-static (MinGW静态)
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg triplet")

#定义Project，一般也无需修改
project(${LIB_PREFIX}Lib VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. 依赖库处理（直接使用 find_package，不再使用复杂的列表循环）
#    注意：如果库提供了 CMake 目标，请使用带命名空间的名称（如 QXlsxQt6::QXlsxQt6）
find_package(Qt6 REQUIRED COMPONENTS Core GuiPrivate)
find_package(QXlsxQt6 REQUIRED)

# 收集依赖目标（注意目标名称：QXlsx 使用的是 QXlsx::QXlsx，不是 QXlsxQt6::QXlsxQt6）
set(DEP_TARGETS
        Qt6::Core          # 显式添加 Qt6 Core 目标
        QXlsx::QXlsx        # QXlsx 库的目标
)
# 收集 find_dependency 代码（用于生成 Config.cmake）
set(DEP_FIND_CODE "")
string(APPEND DEP_FIND_CODE "find_dependency(Qt6 REQUIRED COMPONENTS Core GuiPrivate)\n")
string(APPEND DEP_FIND_CODE "find_dependency(QXlsxQt6 REQUIRED)\n")
message(STATUS "Generated DEP_FIND_CODE: ${DEP_FIND_CODE}")

# ===================================== 以下无需修改 =====================================
# ========== 1. 自动设置安装前缀（如果使用 vcpkg） ==========
if (DEFINED VCPKG_TARGET_TRIPLET AND NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
    if (DEFINED VCPKG_INSTALLED_DIR)
        set(CMAKE_INSTALL_PREFIX "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" CACHE PATH "Installation directory" FORCE)
        message(STATUS "Automatically set CMAKE_INSTALL_PREFIX using VCPKG_INSTALLED_DIR: ${CMAKE_INSTALL_PREFIX}")
    elseif (DEFINED VCPKG_ROOT)
        set(CMAKE_INSTALL_PREFIX "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}" CACHE PATH "Installation directory" FORCE)
        message(STATUS "Automatically set CMAKE_INSTALL_PREFIX using VCPKG_ROOT: ${CMAKE_INSTALL_PREFIX}")
    elseif (CMAKE_PREFIX_PATH)
        list(GET CMAKE_PREFIX_PATH 0 FIRST_PREFIX)
        get_filename_component(VCPKG_ROOT "${FIRST_PREFIX}" DIRECTORY)
        set(CMAKE_INSTALL_PREFIX "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}" CACHE PATH "Installation directory" FORCE)
        message(STATUS "Automatically set CMAKE_INSTALL_PREFIX from CMAKE_PREFIX_PATH: ${CMAKE_INSTALL_PREFIX}")
    else ()
        message(WARNING "Cannot determine vcpkg installation prefix, CMAKE_INSTALL_PREFIX may not be set correctly.")
    endif ()
endif ()

# ========== 2. 源文件设置 ==========
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# ========== 3. 创建库目标 ==========
add_library(${PROJECT_NAME} ${LIB_TYPE} ${SOURCES})

# ========== 4. 生成跨平台的导出头文件（仅动态库需要） ==========
if (LIB_TYPE STREQUAL "SHARED")
    include(GenerateExportHeader)

    set(EXPORT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/${LIB_PREFIX}/${LIB_PREFIX}Export.h")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/${LIB_PREFIX}")

    string(TOUPPER "${LIB_PREFIX}" UPPER_LIB_PREFIX)

    generate_export_header(${PROJECT_NAME}
            EXPORT_MACRO_NAME ${UPPER_LIB_PREFIX}_EXPORT
            EXPORT_FILE_NAME ${EXPORT_HEADER}
    )
    list(APPEND HEADERS ${EXPORT_HEADER})
endif ()

# ========== 5. 链接依赖库 ==========
if (DEP_TARGETS)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${DEP_TARGETS})
endif ()

# ========== 6. 设置包含目录 ==========
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ========== 7. 设置目标属性 ==========
set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${HEADERS}"
        WINDOWS_EXPORT_ALL_SYMBOLS $<IF:$<STREQUAL:${LIB_TYPE},SHARED>,ON,OFF>
)

# ========== 9. 安装配置 ==========
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${LIB_PREFIX}
)

if (LIB_TYPE STREQUAL "SHARED")
    install(FILES ${EXPORT_HEADER} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${LIB_PREFIX})
endif ()

install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION share/${PROJECT_NAME}
)

# ========== 10. 生成包配置文件 ==========
include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonUtilsLibConfig.cmake.in
#        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.base  # 使用临时文件
        INSTALL_DESTINATION share/${PROJECT_NAME}
)

# 在生成的 Config.cmake 中插入依赖查找代码
#file(READ ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake CONFIG_CONTENT)
#string(REPLACE "@PACKAGE_DEPENDENCIES@" "${DEP_FIND_CODE}" CONFIG_CONTENT "${CONFIG_CONTENT}")
#file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake "${CONFIG_CONTENT}")

# 读取基础文件
file(READ ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.base BASE_CONTENT)
# 在 include(CMakeFindDependencyMacro) 之后插入依赖代码
string(REPLACE
        "include(CMakeFindDependencyMacro)"
        "include(CMakeFindDependencyMacro)\n${DEP_FIND_CODE}"
        FINAL_CONTENT "${BASE_CONTENT}"
)
# 写入最终文件
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake "${FINAL_CONTENT}")

write_basic_package_version_file(
        ${PROJECT_NAME}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION share/${PROJECT_NAME}
)

# ========== 11. MSVC运行时库配置 ==========
if (MSVC)
    if (LIB_TYPE STREQUAL "SHARED")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else ()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime for static lib")
    endif ()
endif ()

# ===================================== 使用说明 =====================================
# 1. 在库源码根目录创建 cmake/ 文件夹，并放置 ${PROJECT_NAME}CommonUtilsLibConfig.cmake.in 文件
#    模板内容：
#    @PACKAGE_INIT@
#
#    include(CMakeFindDependencyMacro)
#    @PACKAGE_DEPENDENCIES@
#
#    include("${CMAKE_CURRENT_LIST_DIR}/@PROJECT_NAME@Targets.cmake")
#    check_required_components(${PROJECT_NAME})
#
# 2. 确保 src/ 和 include/ 目录存在，且头文件放在 include/${LIB_PREFIX}/ 下
#