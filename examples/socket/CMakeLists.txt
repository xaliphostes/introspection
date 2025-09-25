project(socket)

include(FetchContent)

message(STATUS "Fetching yhirose/cpp-httplib...")
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3  # Use latest stable version
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(httplib)

message(STATUS "Fetching nlohmann/json...")
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3  # Latest stable version
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)



add_executable(${PROJECT_NAME} main.cxx)

target_link_libraries(${PROJECT_NAME} PRIVATE httplib::httplib)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)


# For threading support (required by httplib)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 wsock32)
    
    # Windows-specific JSON library linking (if using jsoncpp)
    # target_compile_definitions(${PROJECT_NAME} PRIVATE JSON_DLL)
    
elseif(APPLE)
    # macOS may need additional frameworks
    find_library(SECURITY_FRAMEWORK Security)
    if(SECURITY_FRAMEWORK)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_FRAMEWORK})
    endif()
    
elseif(UNIX)
    # Linux-specific settings if needed
    # target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()
