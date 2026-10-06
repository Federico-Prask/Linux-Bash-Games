cmake_minimum_required(VERSION 3.10)
project(thks CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # 源码为无 BOM 的 UTF-8（含中文），MSVC 默认按本地代码页解析会乱码/告警
  add_compile_options(/utf-8)
endif()

include_directories(include)

file(GLOB_RECURSE SRCS "src/*.cpp")
file(GLOB TEST_SRCS "tests/*.cpp")

add_executable(thks main.cpp ${SRCS})

# ---- 单元测试 ----
enable_testing()
list(FILTER SRCS EXCLUDE REGEX ".*/main\\.cpp$")  # 复用引擎全部源文件（除入口）
add_executable(thks_tests ${TEST_SRCS} ${SRCS})
target_include_directories(thks_tests PRIVATE tests)
add_test(NAME thks_tests COMMAND thks_tests)
# 测试读取 docs/*.md 使用相对路径：保证 ctest 从仓库根目录运行
set_tests_properties(thks_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
