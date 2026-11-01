cmake_minimum_required(VERSION 3.15)
project(VoxelEngine C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Find vcpkg packages
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# vcpkg ships Lua as unofficial-lua, other platforms use CMake's FindLua
find_package(unofficial-lua CONFIG QUIET)
if (TARGET unofficial::lua::lua)
    set(VOXEL_LUA_TARGET unofficial::lua::lua)
else()
    find_package(Lua 5.4 REQUIRED)
    add_library(voxel_lua INTERFACE)
    target_include_directories(voxel_lua INTERFACE ${LUA_INCLUDE_DIR})
    target_link_libraries(voxel_lua INTERFACE ${LUA_LIBRARIES})
    set(VOXEL_LUA_TARGET voxel_lua)
endif()

if (NOT unofficial-lua_FOUND AND NOT Lua_FOUND)
    message(FATAL_ERROR "Could not find unofficial-lua (VCPKG) or Lua library")
endif()


add_library(stb IMPORTED INTERFACE)
add_library(unofficial::stb ALIAS stb)

target_include_directories(stb INTERFACE "${CMAKE_CURRENT_LIST_DIR}/libs")

add_executable(VoxelEngine 
    main.cpp 
    Player.cpp
    World.cpp
    ChunkMesh.cpp
    Block.cpp
    Hotbar.cpp
    PauseMenu.cpp
    FontRenderer.cpp
    ParticleSystem.cpp
    ChatSystem.cpp
    Lighting.cpp
    Lighting.h
    Skybox.cpp
    Skybox.h
    CaveGenerator.cpp
    CaveGenerator.h
    Model.cpp
    Model.h
    deps/glad.c
)

target_include_directories(VoxelEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

target_link_libraries(VoxelEngine PRIVATE 
    glfw
    glm::glm
    sol2::sol2
    nlohmann_json::nlohmann_json
    OpenGL::GL
    ${CMAKE_DL_LIBS}
    unofficial::stb
    ${VOXEL_LUA_TARGET}
)

if (TARGET unofficial::lua::lua)
    target_link_libraries(VoxelEngine PRIVATE unofficial::lua::lua)
else()
    target_link_libraries(VoxelEngine PRIVATE ${LUA_LIBRARY})
endif()
