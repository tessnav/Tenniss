cmake_minimum_required(VERSION 3.28)
project(Tenniss)

set(CMAKE_CXX_STANDARD 20)

# Potlačení varování pro čistší log při stahování SQLite
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

include(FetchContent)

# 1. Lokální sestavení SQLite3 databáze
FetchContent_Declare(
        sqlite3_amalgamation
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
)
FetchContent_GetProperties(sqlite3_amalgamation)
if(NOT sqlite3_amalgamation_POPULATED)
    FetchContent_Populate(sqlite3_amalgamation)
    add_library(sqlite3_lib STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3_lib PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
    add_library(SQLite::SQLite3 ALIAS sqlite3_lib)
endif()

# 2. JSON knihovna
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# 3. Asio knihovna
FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

# 4. Crow framework
FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(Crow)

# 5. Spustitelný soubor pouze s main.cpp
add_executable(Tenniss main.cpp
        player.cpp
        player.h)

# 6. Propojení všech knihoven
target_link_libraries(Tenniss
        PRIVATE
        nlohmann_json::nlohmann_json
        Crow::Crow
        SQLite::SQLite3
)

# 7. Nutné systémové knihovny pro Windows
if(WIN32)
    target_link_libraries(Tenniss PRIVATE ws2_32 mswsock)
endif()