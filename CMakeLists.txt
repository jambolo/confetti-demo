cmake_minimum_required (VERSION 3.10)
project(confetti-demo CXX)

option(BUILD_SHARED_LIBS "Build libraries as DLLs" FALSE)

find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Misc REQUIRED)
find_package(Msxmlx REQUIRED)
find_package(Wx REQUIRED)

set(CONFETTI_DEMO_INCLUDE_PATHS
    .
)

set(CONFETTI_DEMO_SOURCES
    stb_image.h
    tiny_obj_loader.h
    confetti-demo.cpp
)
source_group(Sources FILES ${CONFETTI_DEMO_SOURCES})

set(CONFETTI_DEMO_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
)
source_group(Shaders FILES ${CONFETTI_DEMO_SHADER_SOURCES})

add_executable(confetti-demo ${CONFETTI_DEMO_SOURCES})
target_link_libraries(confetti-demo
    Glfwx
    Vkx
    glm::glm
    Vulkan::Vulkan
    Confetti
    nlohmann_json::nlohmann_json
)
target_include_directories(confetti-demo PRIVATE ${CONFETTI_DEMO_INCLUDE_PATHS})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_definitions(confetti-demo
    PRIVATE
        -DNOMINMAX
        -DWIN32_LEAN_AND_MEAN
        -DVC_EXTRALEAN
        -D_CRT_SECURE_NO_WARNINGS
        -D_SECURE_SCL=0
        -D_SCL_SECURE_NO_WARNINGS
)

add_subdirectory(Glfwx)
add_subdirectory(Vkx)
add_subdirectory(Confetti)

# Copy the DLLs to next to the executable
if (WIN32 AND BUILD_SHARED_LIBS)
    find_path(glfw3_DIR "lib/glfw3.lib" PATHS "C:/Program Files/glfw" CACHE)
    if (glfw3_DIR)
        set(glfw3_LIBRARIES "${glfw3_DIR}/lib")
    else()
        message(WARNING "glfw3 was not found.")
    endif()
    add_custom_command(TARGET confetti-demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${glfw3_LIBRARIES}/glfw3.dll" $<TARGET_FILE_DIR:confetti-demo>)
endif()

# Compile the shaders if necessary
find_program(GLSLANGVALIDATOR NAMES glslangValidator glslangValidator.exe)
foreach(GLSL ${CONFETTI_DEMO_SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND ${GLSLANGVALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(
    shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(confetti-demo shaders)
