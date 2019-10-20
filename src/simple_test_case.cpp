#include <string_view>
#include <numeric>
#include "flux.hpp"
#include <cassert>

int main(){
    using namespace std::literals;

    auto text = "testing one two three"sv;
    auto result = std::accumulate(text.begin(), text.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
    auto sub1 = "testing on"sv, sub2 = "e two three"sv;
    auto subResult1 = std::accumulate(sub1.begin(), sub1.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
    auto subResult2 = std::accumulate(sub2.begin(), sub2.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
    // 単語数4。
    assert(result == (subResult1 + subResult2));

    auto sub3 = "testing one"sv, sub4 = " two three"sv;
    auto subResult3 = std::accumulate(sub3.begin(), sub4.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
    auto subResult4 = std::accumulate(sub3.begin(), sub4.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
    // Fluxモノイドはcommutativeではない。
    assert((subResult3 + subResult4) != (subResult4 + subResult3));
}
