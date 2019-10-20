#ifndef BEATING_WC_FLUX_HPP
#define BEATING_WC_FLUX_HPP

#include <variant>
#include <ostream>
#include <compare>
#include <cctype>

enum class CharType {
    IsSpace, NotSpace
};

template<class... Ts>
struct overloaded : Ts ... {
    using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

constexpr auto operator ""_u64(unsigned long long int x) -> uint64_t{
    return x;
}

struct Flux final {
    struct Flux_ {
        int32_t count;
        CharType leftMost;
        CharType rightMost;
        auto operator<=>(Flux_ const&) const = default;
    };

    struct Unknown_ {
        auto operator<=>(Unknown_ const&) const = default;
    };

    std::variant<Flux_, Unknown_> data;
    auto operator<=>(Flux const&) const = default;

    Flux() : data{Unknown_{}} {} // emtpy
    Flux(Flux_ f) : data{std::move(f)} {}

    friend auto operator+(Flux const &lhs, Flux const &rhs) -> Flux {
        return std::visit(overloaded{
                                  [](Unknown_ x, Unknown_) -> Flux {
                                      return {};
                                  },
                                  [](Unknown_, Flux_ y) -> Flux {
                                      return y;
                                  },
                                  [](Flux_ x, Unknown_) -> Flux {
                                      return x;
                                  },
                                  [](Flux_ l, Flux_ r) -> Flux {
                                      auto count = l.rightMost == CharType::NotSpace && r.leftMost == CharType::NotSpace ?
                                                   (l.count + r.count - 1) : (l.count + r.count);
                                      return Flux{Flux_{.count = count, .leftMost = l.leftMost, .rightMost = r.rightMost}};
                                  }
                          },
                          lhs.data, rhs.data);
    }

    template<typename... F>
    decltype(auto) match(F... fs) const {
        return std::visit(overloaded{fs...}, data);
    }
};

struct Counts {
    int32_t charCount = 0;
    Flux wordCount;
    int32_t lineCount = 0;

    friend auto operator+(Counts const &lhs, Counts const &rhs) -> Counts {
        return {.charCount = lhs.charCount + rhs.charCount, .wordCount = lhs.wordCount + rhs.wordCount, .lineCount =
        lhs.lineCount + rhs.lineCount};
    }
};

auto flux(char c) -> Flux {
    return std::isspace(c) != 0 ?
           Flux::Flux_{.count = 0, .leftMost = CharType::IsSpace, .rightMost = CharType::IsSpace} :
           Flux::Flux_{.count = 1, .leftMost = CharType::NotSpace, .rightMost = CharType::NotSpace};
}

auto countChar(char c) -> Counts {
    return {.charCount = 1, .wordCount = flux(c), .lineCount = c == '\n' ? 1 : 0};
}

auto operator<<(std::ostream &os, CharType c) -> std::ostream & {
    switch (c) {
        case CharType::IsSpace:
            return os << "S";
        case CharType::NotSpace:
            return os << "N";
    }
}

auto operator<<(std::ostream &os, Flux const &f) -> std::ostream & {
    return f.match(
            [&](Flux::Flux_ f) -> std::ostream & {
                return os << f.leftMost << f.count << f.rightMost;
            },
            [&](Flux::Unknown_) -> std::ostream & {
                return os << "Unknown";
            });
}

auto operator<<(std::ostream &os, Counts const &c) -> std::ostream & {
    return os << "{charCount = " << c.charCount << ", wordCount = " << c.wordCount << ", lineCount = " << c.lineCount
              << "}";
}
#endif //BEATING_WC_FLUX_HPP
