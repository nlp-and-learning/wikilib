add_executable(benchmark_dump dump_benchmark.cpp)
target_link_libraries(benchmark_dump PRIVATE wikilib::wikilib)

add_executable(benchmark_real_dump real_dump_benchmark.cpp)
target_link_libraries(benchmark_real_dump PRIVATE wikilib::wikilib nlohmann_json::nlohmann_json)
