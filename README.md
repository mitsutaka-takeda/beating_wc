# Beating C with XXX Lines of C++: Wc

Cで書かれた約220行のプログラムwcに匹敵する性能を80行のHaskellで達成するという記事
[Beating C With 80 Lines Of Haskell: Wc](https://chrispenner.ca/posts/wc)があります。プログラムwcは与えられたテキストファイルの文字数・単語数・行数を数えるプログラムです。

この記事ではFluxモノイドという代数構造を使って処理を並列化しています。「テキストファイルの文字数・単語数・行数」を数えるアルゴリズムをモノイドで表現できると、結合律（associativity)のおかげで、テキストファイルを分割して分けた部分を並列で処理することができます。C++の並列化アルゴリズムと相性が良さそうなので試してみます。

この記事のソースコードはこちらです。[レポジトリ](https://github.com/mitsutaka-takeda/beating_wc)

# Fluxモノイド & 単語数の数え上げ

並列化のためにテキストファイルを分割したとき、文字数と行数は部分の文字数と行数を数え上げて結果を足せば良いため、どこで分割しても問題ありません。単語数については分割する場所によっては問題が起こりそうです。単語の途中で分割した場合、２つの部分で両方とも分割された単語の一部を数え、その結果を足すと１つの単語が二重に数えられてしまいます。

部分の左端と右端が単語の区切りになっているかの情報を持てば2つの部分の結果を足すときに単語数の調整ができそうです。そのようなデータと演算を持つ代数構造をFluxモノイドと呼びます。Fluxモノイドの詳細については元記事を参照してください。

FluxモノイドのベースになるFlux代数的データ型はHaskellで以下のように定義されます。

```haskell
data CharType = IsSpace | NotSpace
    deriving Show
data Flux =
    Flux !CharType
         {-# UNPACK #-} !Int
         !CharType
    | Unknown
    deriving Show
```

C++ではenumとvariantで定義することができます。

```cpp
enum class CharType {
    IsSpace, NotSpace
};
struct Flux final {
    struct Flux_ {
        int32_t count;
        CharType leftMost;
        CharType rightMost;
    };

    struct Unknown_ {};

    std::variant<Flux_, Unknown_> data;
};
```

つづいてFlux代数的データ型に対してモノイドのための2項演算子を定義します。HaskellではSemigrouptとMonoid型クラスのインスタンスとして定義します。

```haskell
instance Semigroup Flux where
  Unknown <> x = x
  x <> Unknown = x
  Flux l n NotSpace <> Flux NotSpace n' r = Flux l (n + n' - 1) r
  Flux l n _ <> Flux _ n' r = Flux l (n + n') r

instance Monoid Flux where
  mempty = Unknown
```

C++では演算子のオーバーロードで表現します。足し算も０を単位元としたモノイドなので＋演算子をオーバーロードします。C++23でパターンマッチが入ればHaskellのように書けそうですが、まだないので`std::visit`と`overloaded`イディオムでパターンマッチを実現します。

```cpp
template<class... Ts>
struct overloaded : Ts ... {
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

auto operator+(Flux const &lhs, Flux const &rhs) -> Flux {
        return std::visit(overloaded{
                                  [](Unknown_ x, Unknown_) -> Flux {
                                      return x;
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
```

単位元はFluxのデフォルト・コンストラクタで表現します。

```cpp
struct Flux final {
    //...その他。
    // emtpy
    Flux() : data{Unknown_{}} {}
};
```

文字をFluxデータに変換して"足し合わせる"(FluxとFluxの足し算)と単語数を数えることができます。

```haskell
flux :: Char -> Flux
flux c | isSpace c = Flux IsSpace 0 IsSpace
       | otherwise = Flux NotSpace 1 NotSpace
```

```cpp
auto flux(char c) -> Flux {
    return std::isspace(c) != 0 ?
           Flux::Flux_{.count = 0, .leftMost = CharType::IsSpace, .rightMost = CharType::IsSpace} :
           Flux::Flux_{.count = 1, .leftMost = CharType::NotSpace, .rightMost = CharType::NotSpace};
}
```

モノイドなので分割せずに処理した結果と、分割処理して部分結果を足し合わせた結果が等しくなります。この性質を利用して分割した部分を並列に処理することで性能をあげることができます。

```cpp
auto text = "testing one two three"sv;
auto result = std::accumulate(text.begin(), text.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
auto sub1 = "testing on"sv, sub2 = "e two three"sv;
auto subResult1 = std::accumulate(sub1.begin(), sub1.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
auto subResult2 = std::accumulate(sub2.begin(), sub2.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
// 単語数4。
assert(result == (subResult1 + subResult2));
```

文字数と行数の数え上げもモノイドを定義して行います。ソースコードを参照してください。

# 並列処理

並列処理を行うための準備ができました。

## std parallel algorithm

C++17ではStandard Template Libraryに並列版のアルゴリズムが入りました。

今回の数え上げのアルゴリズムは、文字をモノイドに変換してそれを合計します。algorithmの一覧を眺めると[std::transform_reduce::cppreference](https://en.cppreference.com/w/cpp/algorithm/transform_reduce)という名前のアルゴリズムがあります。名前的にはこれが使えそうです。

`std::transform_reduce`の並列版は`ForwardIterator`コンセプトを要求します。ファイルの中身に`RandomAccessIterator`でアクセスできるようにmemory mapped fileを使用します。

```cpp
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
```

並列版のアルゴリズムを利用したいため、`ExcecutionPolicy`には`std::execution::parallel_unsequenced_policy `を設定します。`Counts`は説明を省略した数え上げのためのモノイドです。文字を`countChar`で`Counts`モノイドにマップしそれを＋演算子で合計していきます。

これで並列版のwcができました！このプログラムを動かしてみると文字数や行数は何度実行しても同じ数になりまずが単語数は実行毎に異なる値になります。。。`std::transform_reduce`の使用を確認すると２項演算子は結合律(associativity)と交換律(commutativity)を満たさなければいけません。

`Flux`モノイドは結合律は満たしますが交換律は満たしません。そのため`std::transform_reduce`の動作は非決定的になります。

```cpp
auto sub3 = "testing one"sv, sub4 = " two three"sv;
auto subResult3 = std::accumulate(sub3.begin(), sub4.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
auto subResult4 = std::accumulate(sub3.begin(), sub4.end(), Flux{}, [](Flux f, char c){ return f + flux(c); });
// Fluxモノイドはcommutativeではない。
assert((subResult3 + subResult4) != (subResult4 + subResult3));
```

## Parallel版accumulate

いま必要な実装は結合律を要求して交換律を要求しない（分割した部分間の演算順序を変更しない）アルゴリズムです。`std::accumulate`(fold left)がそれにあたりますがParallel版は存在しないため自作することにします。

処理はスレッドプールで分割します。スレッドプールの実装には`CppCoro`を使用します。区間を半分に分割して最初の半分をスレッドプール上で、後半の半分を自身のスレッド上で処理を実行し結果をまとめる分割統治法です。

```cpp
template<typename RandomIt, typename T, typename BinaryOperation, typename Projection>
auto
parallel_accumulate(cppcoro::static_thread_pool &tp, RandomIt first, RandomIt last, T init, BinaryOperation op, Projection p) -> cppcoro::task<T> {
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
```

メインは`std::transform_reduce`とほぼ同じです。スレッドプールを初期化してアルゴリズムを呼び出します。

```cpp
int main(int argc, char *argv[]) {
    // transform_reduceと同じ。
    // ...
    cppcoro::static_thread_pool threadPool;
    auto result = cppcoro::sync_wait(
            parallel_accumulate(threadPool, r_mmap.begin(), r_mmap.end(), Counts{}, std::plus<>{}, countChar)
    );

    std::cout << result << std::endl;
}
```

# 性能

約780MBのテキストファイルに対してリファレンス実装のwcと`parallel_accumulate`版のwcを実行すると10回の経過時間(wall-clock time)の平均は以下の通りです。

| リファレンスwc | `parallel_accumulate` |
|------------|-----------------------|
|6018 msec   | 3043 msec             |


`parallel_accumulate`を使用したバージョンの実装はリファレンスのwcに対して経過時間比で約50%程短縮できています。元記事のHaskell版の短縮率が41％(≒(2.07sec-1.23sec)/2.07sec)ほどなので、それよりも短縮できています。

コードの行数は自前の`parallel_accumulate`の20行を含め150行ほどです。

# 最後に

この記事ではC++でモノイドを実装する方法とモノイドを使用して処理の並列化を実装する方法を紹介しました。モノイドのような代数構造を使用してロジックを表現すると並列版アルゴリズムが適用でき高い抽象度を保ったまま並列化の恩恵を受けることが可能です。

代数構造＆アルゴリズムを使用するときはアルゴリズムが要求する性質を代数構造が持っていることを確認しましょう。

# 参考URL

- [Beating C With 80 Lines Of Haskell: Wc](https://chrispenner.ca/posts/wc)
- [That `overloaded` Trick: Overloading Lambdas in C++17](https://dev.to/tmr232/that-overloaded-trick-overloading-lambdas-in-c17)
- [mio: Cross-platform C++11 header-only library for memory mapped file IO](https://github.com/mandreyel/mio)
- [CppCoro - A coroutine library for C++](https://github.com/lewissbaker/cppcoro)
- [std::transform_reduce::cppreference](https://en.cppreference.com/w/cpp/algorithm/transform_reduce)