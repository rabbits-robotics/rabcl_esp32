cmake_minimum_required(VERSION 3.16.0)
# 親ディレクトリ（rabcl_esp32 本体）をコンポーネントとして拾う
# （PlatformIO は src/ を EXTRA_COMPONENT_DIRS で渡してくるので、上書きせず追加する）
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(can_loop)
