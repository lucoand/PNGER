cmake_minimum_required(VERSION 3.10)
project(pnger)

set(CMAKE_C_STANDARD 99)

# Find GLFW
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

# Include directories
include_directories(
	${GLFW_INCLUDE_DIRS}
	glad/include
	src
)

# Add glad source
set(GLAD_SOURCES glad/src/gl.c)

set(SOURCES 
	src/main.c
	src/png.c
	${GLAD_SOURCES}
)

add_executable(pnger ${SOURCES})

# Link against glfw, math, and dl
target_link_libraries(pnger ${GLFW_LIBRARIES} z m dl)
