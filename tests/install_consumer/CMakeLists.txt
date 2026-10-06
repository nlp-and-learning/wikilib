cmake_minimum_required(VERSION 3.20)
project(wikilib_installed_consumer LANGUAGES CXX)
# The imported target must upgrade this consumer to the library's required C++23.
set(CMAKE_CXX_STANDARD 17)
find_package(wikilib CONFIG REQUIRED)
add_executable(installed_consumer main.cpp)
target_link_libraries(installed_consumer PRIVATE wikilib::wikilib)
enable_testing()
add_test(NAME installed_consumer_runs COMMAND installed_consumer)
