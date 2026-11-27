include_directories(include)
find_package(ALSA REQUIRED)

file(GLOB MY_SOURCES "src/*.c")
add_executable( ENSC351_GALLERY ${MY_SOURCES})

target_link_libraries( ENSC351_GALLERY PRIVATE asound hal gpiod m)

add_custom_command(TARGET ENSC351_GALLERY POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE:ENSC351_GALLERY>"
        "$ENV{HOME}/public/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_SOURCE_DIR}/wave-files"
        "~/public/wave-files" 
    COMMENT "Copying exec to public")