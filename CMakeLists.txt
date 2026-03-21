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

option(USE_BUNDLED_LIBHV "Build libhv from third_party/libhv" ON)

# Include directories
include_directories(include)
include_directories(third_party/cpp-httplib)
include_directories(third_party/jwt-cpp/include)
include_directories(third_party/ormpp)
include_directories(third_party/ormpp/ormpp)
include_directories(third_party/ormpp/iguana)

if(USE_BUNDLED_LIBHV)
    set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
    set(WITH_OPENSSL ON CACHE BOOL "" FORCE)
    set(WITH_HTTP ON CACHE BOOL "" FORCE)
    set(WITH_HTTP_SERVER ON CACHE BOOL "" FORCE)
    set(WITH_HTTP_CLIENT ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/libhv EXCLUDE_FROM_ALL)
    include_directories(third_party/libhv/include)
else()
    find_path(HV_INCLUDE_DIR hv/hv.h)
    find_library(HV_LIBRARY hv)
    if(NOT HV_INCLUDE_DIR OR NOT HV_LIBRARY)
        message(FATAL_ERROR "libhv not found! Please install it first.")
    endif()
    include_directories(${HV_INCLUDE_DIR})
endif()

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
    src/config/AgentConfig.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/agentic/EmbeddingClient.cpp
    src/agentic/EmbeddingFactory.cpp
    src/agentic/QdrantClient.cpp
    src/agentic/RagRetriever.cpp
    src/agentic/InterviewPipeline.cpp
    src/agentic/InterviewPipelineDefaults.cpp
    src/service/LoginService.cpp
    src/router/LoginRouter.cpp
    src/service/ChatService.cpp
    src/service/InterviewService.cpp
    src/service/SettingService.cpp
    src/service/ChatMySqlStore.cpp
    src/service/MemoryIndexService.cpp
    src/router/ChatRouter.cpp
    src/router/InterviewRouter.cpp
    src/router/SettingRouter.cpp
    src/router/DebugRouter.cpp
)

add_executable(app ${SOURCE_FILES})

# Link libraries (add crypto for JWT)
target_compile_definitions(app PRIVATE ORMPP_ENABLE_MYSQL)
target_compile_definitions(app PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(app hv_static ssl crypto mysqlclient)
else()
    target_link_libraries(app ${HV_LIBRARY} ssl crypto mysqlclient)
endif()
set_target_properties(app PROPERTIES LINK_FLAGS "-Wl,-z,muldefs")

add_executable(bailian_smoketest
    test/bailian_smoketest.cpp
    src/config/Dotenv.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
)
target_compile_definitions(bailian_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(bailian_smoketest hv_static ssl crypto)
else()
    target_link_libraries(bailian_smoketest ${HV_LIBRARY} ssl crypto)
endif()

add_executable(sendmessage_smoketest
    test/sendmessage_smoketest.cpp
    src/config/Dotenv.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/service/ChatService.cpp
)
target_compile_definitions(sendmessage_smoketest PRIVATE ORMPP_ENABLE_MYSQL)
target_compile_definitions(sendmessage_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(sendmessage_smoketest hv_static ssl crypto mysqlclient)
else()
    target_link_libraries(sendmessage_smoketest ${HV_LIBRARY} ssl crypto mysqlclient)
endif()
set_target_properties(sendmessage_smoketest PROPERTIES LINK_FLAGS "-Wl,-z,muldefs")

add_executable(llm_factory_smoketest
    test/llm_factory_smoketest.cpp
    src/config/Dotenv.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
)
target_compile_definitions(llm_factory_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(llm_factory_smoketest hv_static ssl crypto)
else()
    target_link_libraries(llm_factory_smoketest ${HV_LIBRARY} ssl crypto)
endif()

add_executable(model_switch_smoketest
    test/model_switch_smoketest.cpp
    src/config/Dotenv.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/service/SettingService.cpp
)
target_compile_definitions(model_switch_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(model_switch_smoketest hv_static ssl crypto)
else()
    target_link_libraries(model_switch_smoketest ${HV_LIBRARY} ssl crypto)
endif()

add_executable(embedding_smoketest
    test/embedding_smoketest.cpp
    src/config/Dotenv.cpp
    src/config/AgentConfig.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/agentic/EmbeddingClient.cpp
    src/agentic/EmbeddingFactory.cpp
)
target_compile_definitions(embedding_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(embedding_smoketest hv_static ssl crypto)
else()
    target_link_libraries(embedding_smoketest ${HV_LIBRARY} ssl crypto)
endif()

add_executable(qdrant_smoketest
    test/qdrant_smoketest.cpp
    src/config/Dotenv.cpp
    src/config/AgentConfig.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/agentic/EmbeddingClient.cpp
    src/agentic/EmbeddingFactory.cpp
    src/agentic/QdrantClient.cpp
)
target_compile_definitions(qdrant_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(qdrant_smoketest hv_static ssl crypto)
else()
    target_link_libraries(qdrant_smoketest ${HV_LIBRARY} ssl crypto)
endif()

add_executable(memory_index_smoketest
    test/memory_index_smoketest.cpp
    src/config/Dotenv.cpp
    src/config/AgentConfig.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/agentic/EmbeddingClient.cpp
    src/agentic/EmbeddingFactory.cpp
    src/agentic/QdrantClient.cpp
    src/service/ChatMySqlStore.cpp
    src/service/MemoryIndexService.cpp
    src/service/ChatService.cpp
)
target_compile_definitions(memory_index_smoketest PRIVATE ORMPP_ENABLE_MYSQL)
target_compile_definitions(memory_index_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(memory_index_smoketest hv_static ssl crypto mysqlclient)
else()
    target_link_libraries(memory_index_smoketest ${HV_LIBRARY} ssl crypto mysqlclient)
endif()
set_target_properties(memory_index_smoketest PROPERTIES LINK_FLAGS "-Wl,-z,muldefs")

add_executable(sse_hold_smoketest
    test/sse_hold_smoketest.cpp
)
target_compile_definitions(sse_hold_smoketest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(sse_hold_smoketest hv_static ssl crypto)
else()
    target_link_libraries(sse_hold_smoketest ${HV_LIBRARY} ssl crypto)
endif()

add_executable(rag_hit_rate_eval
    test/rag_hit_rate_eval.cpp
    src/config/Dotenv.cpp
    src/config/AgentConfig.cpp
    src/ai/LLMFactory.cpp
    src/ai/BailianLLM.cpp
    src/agentic/EmbeddingClient.cpp
    src/agentic/EmbeddingFactory.cpp
    src/agentic/QdrantClient.cpp
    src/agentic/RagRetriever.cpp
)
target_compile_definitions(rag_hit_rate_eval PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
if(USE_BUNDLED_LIBHV)
    target_link_libraries(rag_hit_rate_eval hv_static ssl crypto)
else()
    target_link_libraries(rag_hit_rate_eval ${HV_LIBRARY} ssl crypto)
endif()
