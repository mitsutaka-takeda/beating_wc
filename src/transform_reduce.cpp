#include <iostream>
#include <numeric>
#include <execution>
#include <mio/mmap.hpp>
#include "flux.hpp"

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

    auto result = std::transform_reduce(std::execution::par_unseq, r_mmap.begin(), r_mmap.end(), Counts{}, std::plus<>{}, countChar);

    std::cout << result << std::endl;
}
