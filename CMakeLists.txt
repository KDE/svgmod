project(svgmod)
cmake_minimum_required(VERSION 2.8)

find_package(Qt4 REQUIRED)

include_directories(${QT_INCLUDES})

set(svgmod_SRCS svgmod.cpp )
add_executable(svgmod ${svgmod_SRCS})
target_link_libraries(svgmod ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTXML_LIBRARY})
install(TARGETS svgmod RUNTIME DESTINATION bin)
