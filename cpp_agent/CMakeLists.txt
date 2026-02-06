cmake_minimum_required(VERSION 3.15)
project(cpp_agent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖库
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 查找CURL库
pkg_check_modules(CURL libcurl)
if(NOT CURL_FOUND)
    # 如果pkg-config找不到，尝试直接查找
    find_library(CURL_LIBRARIES curl)
    find_path(CURL_INCLUDE_DIRS curl/curl.h)
    if(CURL_LIBRARIES AND CURL_INCLUDE_DIRS)
        set(CURL_FOUND TRUE)
        message(STATUS "Found CURL: ${CURL_LIBRARIES}")
    else()
        message(FATAL_ERROR "CURL library not found. Please install libcurl4-openssl-dev")
    endif()
else()
    message(STATUS "Found CURL via pkg-config: ${CURL_LIBRARIES}")
endif()

include_directories(${CURL_INCLUDE_DIRS})

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/llm
    ${CMAKE_CURRENT_SOURCE_DIR}/tts
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# 源文件
set(SOURCES
    main.cpp
    memory/long_term.cpp
    memory/short_term.cpp
    llm/llm.cpp
    tts/tts.cpp
    utils/logger.cpp
    utils/config.cpp
)

# 头文件
set(HEADERS
    memory/long_term.h
    memory/short_term.h
    llm/llm.h
    tts/tts.h
    utils/logger.h
    utils/config.h
)

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    Threads::Threads
    ${CURL_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O3 -DNDEBUG)
    message(STATUS "Build type: Release (optimized)")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -g -O0)
    message(STATUS "Build type: Debug")
endif()

# 注意：安装规则已移除，因为部署脚本会手动处理文件复制
# 这样可以避免CMake安装路径的问题，部署脚本更灵活

# 复制静态文件和配置文件到构建目录（用于开发测试）
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/static DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
# 复制配置文件示例（如果config.json不存在则复制示例文件）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.json)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
else()
    # 先复制文件，然后重命名
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.json.example 
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/config.json.example 
                ${CMAKE_CURRENT_BINARY_DIR}/config.json)
endif()
