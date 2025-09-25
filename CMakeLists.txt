project(edat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

add_subdirectory(samples)
add_subdirectory(src edat)

#add_library(edat)
target_include_directories(edat PUBLIC include)

