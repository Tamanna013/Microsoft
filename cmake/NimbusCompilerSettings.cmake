# Shared compiler settings for all NimbusFS targets

function(nimbus_apply_compiler_settings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wpedantic
            -Wformat=2
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()

    if(CMAKE_EXPORT_COMPILE_COMMANDS)
        set_target_properties(${target} PROPERTIES
            CXX_CLANG_TIDY "clang-tidy"
        )
    endif()
endfunction()
