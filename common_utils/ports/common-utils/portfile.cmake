# 使用绝对路径
set(SOURCE_PATH "D:/Documents/CLionProjects/Qt/Projects/common_utils")

# 配置 CMake
vcpkg_configure_cmake(
        SOURCE_PATH ${SOURCE_PATH}
        PREFER_NINJA
        OPTIONS
        -DLIB_TYPE=SHARED
)

# 构建和安装
vcpkg_install_cmake()

# 清理调试目录的重复文件
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# 复制 PDB 文件
vcpkg_copy_pdbs()

# 安装 usage 文件
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

# 处理版权文件
if(NOT EXISTS "${SOURCE_PATH}/LICENSE")
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "MIT License\n")
else()
    file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
endif()
