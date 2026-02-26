cmake_minimum_required(VERSION 3.15)   # 3.15+ is safe for C++17, most people have it

project(WallScraper 
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # no gnu extensions, keep it clean

# If you want NO console window at all (silent background gremlin mode)
add_executable(wallScraper WIN32 wallscraper.cpp)

# But for now, keep console so you can see errors / debug logs during dev
# add_executable(wallScraper wallscraper.cpp)

# Link the Windows libs you need (most are implicit, but explicit is clearer & safer)
target_link_libraries(wallScraper PRIVATE
    user32      # SystemParametersInfoW, GetDC, etc.
    gdi32       # BitBlt, CreateCompatibleBitmap, etc.
    advapi32    # RegOpenKeyExW, RegSetValueExW
    shell32     # SHGetKnownFolderPath
    ole32       # CoTaskMemFree
    # kernel32 is always linked automatically, no need to list
)

# Optional: Hide console in Release builds only (good compromise)
# if(CMAKE_BUILD_TYPE MATCHES Release)
#     set_target_properties(wallScraper PROPERTIES WIN32_EXECUTABLE TRUE)
# endif()

# If you hate warnings (or want pedantic mode)
# target_compile_options(wallScraper PRIVATE /W4 /WX)   # MSVC style
# or for gcc/clang if cross-compiling: -Wall -Wextra -Werror

# Optional: Define some preprocessor stuff if you want
# target_compile_definitions(wallScraper PRIVATE 
#     _UNICODE 
#     UNICODE
# )