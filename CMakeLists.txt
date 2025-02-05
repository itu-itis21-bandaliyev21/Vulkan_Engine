cmake_minimum_required(VERSION 3.22.1)

project(Nyxis_Engine)
set(PROJECT Nyxis_Engine)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TBB REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file (GLOB_RECURSE SOURCES source/*.cpp libs/entt/*.cpp libs/stbimage/*.cpp libs/tinyobjloader/*.cpp libs/imgui/imgui.cpp 
        libs/imgui/imgui_demo.cpp libs/imgui/imgui_draw.cpp libs/imgui/imgui_tables.cpp libs/imgui/imgui_widgets.cpp 
        libs/imgui/misc/cpp/imgui_stdlib.cpp libs/imgui/imgui libs/imgui/backends/imgui_impl_glfw.cpp libs/imgui/backends/imgui_impl_vulkan.cpp
        libs/ImGuizmo/GraphEditor.cpp libs/ImGuizmo/ImCurveEdit.cpp libs/ImGuizmo/ImGradient.cpp libs/ImGuizmo/ImGuizmo.cpp
        libs/ImGuizmo/ImSequencer.cpp libs/imgui-node-editor/imgui_canvas.cpp libs/imgui-node-editor/imgui_node_editor.cpp
        libs/imgui-node-editor/imgui_node_editor_api.cpp libs/imgui-node-editor/crude_json.cpp)
file (GLOB_RECURSE HEADERS source/*/*.hpp )

# set filter for shaders in shaders folder
file(GLOB_RECURSE SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
source_group("Shaders" FILES ${SHADER_FILES})

file(GLOB_RECURSE Core "${CMAKE_CURRENT_SOURCE_DIR}/source/Core/*")
file(GLOB_RECURSE Events "${CMAKE_CURRENT_SOURCE_DIR}/source/Events/*")
file(GLOB_RECURSE Graphics "${CMAKE_CURRENT_SOURCE_DIR}/source/Graphics/*")
file(GLOB_RECURSE NyxisUI "${CMAKE_CURRENT_SOURCE_DIR}/source/NyxisUI/*")
file(GLOB_RECURSE RenderSystems "${CMAKE_CURRENT_SOURCE_DIR}/source/RenderSystems/*")
file(GLOB_RECURSE Scene "${CMAKE_CURRENT_SOURCE_DIR}/source/Scene/*")
file(GLOB_RECURSE Utils "${CMAKE_CURRENT_SOURCE_DIR}/source/Utils/*")

source_group("Core" FILES ${Core})
source_group("Events" FILES ${Events})
source_group("Graphics" FILES ${Graphics})
source_group("NyxisUI" FILES ${NyxisUI})
source_group("RenderSystems" FILES ${RenderSystems})
source_group("Scene" FILES ${Scene})
source_group("Utils" FILES ${Utils})

add_executable(${PROJECT} ${SOURCES} ${HEADERS} ${SHADER_FILES} ${Core})

target_include_directories(${PROJECT} PUBLIC source libs libs/imgui libs/imgui/backends libs/stbimage)
target_link_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs)
target_link_libraries(${PROJECT} PUBLIC Vulkan::Vulkan glfw )

option(GLI_TEST_ENABLE "Build unit tests" OFF)
add_subdirectory(libs/spdlog)
add_subdirectory(libs/gli)
add_subdirectory(libs/assimp)
target_link_libraries(${PROJECT} PRIVATE spdlog gli assimp TBB::tbb)

set_property(TARGET ${PROJECT} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT})
