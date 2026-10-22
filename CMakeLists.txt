cmake_minimum_required(VERSION 3.17)
project(chatLab CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)     # 生成 compile_commands.json，给 clangd 用

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)    # 这个项目以调试为主：不开优化、调试信息最全
endif()

add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-ferror-limit=5)     # clang：最多显示 5 条错误
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-fmax-errors=5)      # g++：同上
endif()

# 需要查内存问题的时候：cmake ... -DENABLE_ASAN=ON
option(ENABLE_ASAN "Enable AddressSanitizer and UBSan" OFF)
if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)      # 来自 apt 的 nlohmann-json3-dev
find_package(ZLIB REQUIRED)


# ==================================================
# 公共代码
# protocol / socket_io 等
# ==================================================
add_library(chat_core #创建一个叫chatcore的库，把common里的东西单独编译
  common/net.cpp
  common/protocol.cpp
  client/client.cpp
)
target_include_directories(chat_core PUBLIC # 头文件可以从当前项目根目录开始查找。允许如 #include "common/protocol.cpp"
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(chat_core PUBLIC
  Threads::Threads
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
  ZLIB::ZLIB
)

# ==================================================
# Server
# ==================================================
add_executable(chat_server
  server/session_manager.h
  server/session_manager.cpp
  server/server.h
  server/server.cpp
  server/main.cpp)

target_link_libraries(chat_server PRIVATE chat_core)


# ==================================================
# Client
# ==================================================
add_executable(chat_client
  client/client.h
  client/client.cpp
  client/main.cpp)

target_link_libraries(chat_client PRIVATE chat_core)


# ==================================================
# Tests
# ==================================================
enable_testing()
add_executable(basic_multithread 
  tests/basic_multithread.cpp
)

target_link_libraries(basic_multithread PRIVATE chat_core)

add_test(NAME basic_multithread COMMAND basic_multithread)


# 重新配置 CMake
#cmake -S . -B build
# 重新编译
#cmake --build build