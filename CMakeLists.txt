cmake_minimum_required(VERSION 3.21)
project(dms_desktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PostgreSQL QUIET)
if(NOT PostgreSQL_FOUND)
    find_program(DMS_PG_CONFIG pg_config REQUIRED)
    execute_process(COMMAND ${DMS_PG_CONFIG} --includedir
        OUTPUT_VARIABLE DMS_PG_INCLUDE_HINT OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${DMS_PG_CONFIG} --libdir
        OUTPUT_VARIABLE DMS_PG_LIBRARY_HINT OUTPUT_STRIP_TRAILING_WHITESPACE)
    find_path(DMS_PG_INCLUDE_DIR libpq-fe.h HINTS ${DMS_PG_INCLUDE_HINT} REQUIRED)
    find_library(DMS_PG_LIBRARY NAMES pq HINTS ${DMS_PG_LIBRARY_HINT} REQUIRED)
    add_library(PostgreSQL::PostgreSQL UNKNOWN IMPORTED)
    set_target_properties(PostgreSQL::PostgreSQL PROPERTIES
        IMPORTED_LOCATION "${DMS_PG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DMS_PG_INCLUDE_DIR}")
endif()
find_package(bsoncxx REQUIRED CONFIG)
find_package(mongocxx REQUIRED CONFIG)

add_executable(dms_app
    app/main.cpp
    app/main_window.cpp
    app/main_window.hpp
    app/pg_database.cpp
    app/pg_database.hpp
    sprint5/mongo_crud.cpp
    sprint5/mongo_schema.cpp
)

target_include_directories(dms_app PRIVATE app sprint5)
target_link_libraries(dms_app PRIVATE
    Qt6::Widgets
    PostgreSQL::PostgreSQL
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
)
