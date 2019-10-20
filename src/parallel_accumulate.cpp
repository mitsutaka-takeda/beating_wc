#include <cppcoro/task.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/schedule_on.hpp>
#include <iostream>
#include <mio/mmap.hpp>
#include <functional>
#include "flux.hpp"

template<typename RandomIt, typename T, typename BinaryOperation, typename Projection>
auto
parallel_accumulate(cppcoro::static_thread_pool &tp, RandomIt first, RandomIt last, T init,
                    BinaryOperation op, Projection p) -> cppcoro::task<T> {
    const auto count = std::distance(first, last);
    if (count <= 100000) {
        for (; first != last; ++first) {
            init = std::invoke(op, init, std::invoke(p, *first));
        }
        co_return init;
    } else {
        auto half = count / 2;
        auto[first, second] = co_await cppcoro::when_all(
                cppcoro::schedule_on(tp, parallel_accumulate(tp, first, first + half, init, op, p)),
                parallel_accumulate(tp, first + half, last, init, op, p)
        );
        co_return op(first, second);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "word_count <path-to-file>" << std::endl;
        exit(1);
    }

    std::error_code error;
    auto r_mmap = mio::make_mmap_source(argv[1], error);
    if (error) {
        std::cerr << "failed to open the file: " << argv[1] << " for " << error << std::endl;
        exit(1);
    }

    cppcoro::static_thread_pool threadPool;
    auto result = cppcoro::sync_wait(
            parallel_accumulate(threadPool, r_mmap.begin(), r_mmap.end(), Counts{}, std::plus<>{}, countChar)
    );

    std::cout << result << std::endl;
}
