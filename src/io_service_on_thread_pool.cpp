#include <filesystem>
#include <string_view>
#include <iostream>
#include <numeric>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/schedule_on.hpp>

#include "flux.hpp"

auto processEvents(cppcoro::io_service& ioService) -> cppcoro::task<>{
    ioService.process_events();
    co_return;
}



auto countsTask(
        cppcoro::io_service &ioService,
        std::string_view path,
        uint64_t offset,
        uint64_t limit
) -> cppcoro::task<Counts> {
    auto file = cppcoro::read_only_file::open(ioService, path, cppcoro::file_share_mode::read,
                                              cppcoro::file_buffering_mode::sequential);

    Counts counts;

    constexpr int bufferSize = 1024;
    char buffer[bufferSize];
    size_t bytesRead;

    do {
        bytesRead = co_await file.read(offset, buffer, sizeof(buffer));
        auto const until = limit - offset;
        counts = std::accumulate(buffer, buffer + std::min(bytesRead, until), counts,
                                 [](Counts const &c, char ch) { return c + countChar(ch); });
        offset += bytesRead;
    } while (bytesRead > 0 && offset < limit);

    co_return counts;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "word_count <path-to-file>" << std::endl;
        exit(1);
    }

    auto fileSize = std::filesystem::file_size(argv[1]);
    cppcoro::static_thread_pool threadPool;

    auto const numberOfCores = threadPool.thread_count();
    auto ioService = cppcoro::io_service{numberOfCores};

    auto[result, u] = cppcoro::sync_wait(cppcoro::when_all(
            [&, chunkSize = fileSize / numberOfCores, fileSize]() -> cppcoro::task<std::vector<Counts>> {
                cppcoro::io_work_scope scope{ioService};
                std::vector<cppcoro::task<Counts>> tasks(numberOfCores);
                uint64_t beg = 0;
                for (auto i = 0_u64; i < numberOfCores - 1; ++i) {
                    tasks[i] = countsTask(ioService, argv[1], beg, beg + chunkSize);
                    beg += chunkSize;
                }
                tasks.back() = countsTask(ioService, argv[1], beg, fileSize);
                auto result = co_await cppcoro::when_all(std::move(tasks));

                co_return result;
            }(),
            [&]() -> cppcoro::task<> {
                std::vector<cppcoro::task<>> tasks;
                for(auto i = 0_u64; i < numberOfCores; ++i){
                    tasks.push_back(cppcoro::schedule_on(threadPool, processEvents(ioService)));
                }
                co_await cppcoro::when_all(std::move(tasks));
            }()));

    std::cout << std::accumulate(result.begin(), result.end(), Counts{}, std::plus<>{}) << std::endl;

}
