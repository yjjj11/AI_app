cmake_minimum_required(VERSION 3.10)
project(My_AIapp)

set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

execute_process(COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)

# Find libhv
find_path(HV_INCLUDE_DIR hv/hv.h)
find_library(HV_LIBRARY hv)

if(NOT HV_INCLUDE_DIR OR NOT HV_LIBRARY)
    message(FATAL_ERROR "libhv not found! Please install it first.")
endif()

# Include directories
include_directories(${HV_INCLUDE_DIR})
include_directories(include)
include_directories(third_party/jwt-cpp/include)
include_directories(third_party/ormpp)
include_directories(third_party/ormpp/ormpp)
include_directories(third_party/ormpp/iguana)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SQLITE3 OFF CACHE BOOL "" FORCE)
set(ENABLE_MYSQL ON CACHE BOOL "" FORCE)

add_subdirectory(third_party/ormpp EXCLUDE_FROM_ALL)
include_directories(${MYSQL_INCLUDE_DIR})

# Source files
set(SOURCE_FILES
    src/main.cpp 
    src/HttpServer.cpp
    src/config/Dotenv.cpp
    src/service/LoginService.cpp
    src/router/LoginRouter.cpp
    src/service/ChatService.cpp
    src/service/ChatMySqlStore.cpp
    src/router/ChatRouter.cpp
)

add_executable(app ${SOURCE_FILES})

# Link libraries (add crypto for JWT)
target_compile_definitions(app PRIVATE ORMPP_ENABLE_MYSQL)
target_link_libraries(app ${HV_LIBRARY} crypto mysqlclient)
set_target_properties(app PROPERTIES LINK_FLAGS "-Wl,-z,muldefs")
