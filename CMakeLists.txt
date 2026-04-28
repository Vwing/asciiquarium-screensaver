cmake_minimum_required(VERSION 3.10)
project(AsciiquariumScreensaver)

set(CMAKE_CXX_STANDARD 11)

# ── Rendering application (AsciiquariumApp.exe) ──────────────────────────────
add_executable(AsciiquariumApp WIN32 AsciiquariumApp.cpp)

target_link_libraries(AsciiquariumApp
    user32
    gdi32
)

if (MSVC)
    set_target_properties(AsciiquariumApp PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# ── Screensaver wrapper (Asciiquarium.scr) ────────────────────────────────────
add_executable(Asciiquarium WIN32
    AsciiquariumWrapper.cpp
    screensaver.def
    screensaver.rc
)

set_target_properties(Asciiquarium PROPERTIES
    SUFFIX ".scr"
)

target_link_libraries(Asciiquarium
    scrnsave
    user32
    gdi32
    comctl32
)

if (MSVC)
    set_target_properties(Asciiquarium PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# Install both next to each other so the wrapper can find the app
install(TARGETS Asciiquarium AsciiquariumApp
    DESTINATION ${CMAKE_INSTALL_PREFIX})
