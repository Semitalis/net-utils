include(FetchContent)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(googletest
    GIT_REPOSITORY
        https://github.com/google/googletest.git
    GIT_TAG
        v1.13.0
    GIT_SHALLOW
        1
    UPDATE_DISCONNECTED
        1
)

FetchContent_MakeAvailable(googletest)
