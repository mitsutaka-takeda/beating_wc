cmake_minimum_required(VERSION 3.14)
project(beating_wc)

set(CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)
find_package(mio CONFIG REQUIRED)
add_subdirectory(../cppcoro cppcoro)

add_executable(wc src/wc/wc_reference.cpp src/wc/getopt.cpp src/wc/getopt.hpp)

add_executable(io_service_per_thread src/io_service_per_thread.cpp src/flux.hpp)
target_link_libraries(io_service_per_thread PRIVATE mio::mio mio::mio_base mio::mio_full_winapi cppcoro::cppcoro Threads::Threads synchronization.lib ws2_32.lib)
target_compile_features(io_service_per_thread PUBLIC cxx_std_20)

add_executable(io_service_on_thread_pool src/io_service_on_thread_pool.cpp src/flux.hpp)
target_link_libraries(io_service_on_thread_pool PRIVATE mio::mio mio::mio_base mio::mio_full_winapi cppcoro::cppcoro Threads::Threads synchronization.lib ws2_32.lib)
target_compile_features(io_service_on_thread_pool PUBLIC cxx_std_20)

add_executable(parallel_accumulate src/parallel_accumulate.cpp src/flux.hpp)
target_link_libraries(parallel_accumulate PRIVATE mio::mio mio::mio_base mio::mio_full_winapi cppcoro::cppcoro Threads::Threads synchronization.lib ws2_32.lib)
target_compile_features(parallel_accumulate PUBLIC cxx_std_20)

add_executable(simple_test_case src/simple_test_case.cpp src/flux.hpp)
target_compile_features(simple_test_case PUBLIC cxx_std_20)

add_executable(transform_reduce src/transform_reduce.cpp src/flux.hpp)
target_link_libraries(transform_reduce PRIVATE mio::mio mio::mio_base mio::mio_full_winapi)
target_compile_features(transform_reduce PUBLIC cxx_std_20)