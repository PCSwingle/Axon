#pragma once

#include <filesystem>
#include <vector>

struct GeneratedType;

std::vector<std::string> split(const std::string& str, const std::string& separator);

std::string readStdin();

std::string readFile(const std::filesystem::path& filename);

size_t combineHash(size_t seed, size_t other);

template<typename... Ts>
struct std::hash<std::tuple<Ts...> > {
    size_t operator()(std::tuple<Ts...> const& tup) const noexcept {
        return std::apply([]<typename... T0>(T0 const&... elems) {
                              size_t seed = 0;
                              ((seed = combineHash(std::hash<std::decay_t<T0> >{}(elems), seed)),
                                  ...
                              );
                              return seed;
                          },
                          tup);
    }
};

template<typename... Ts>
struct std::hash<std::vector<Ts...> > {
    size_t operator()(std::vector<Ts...> const& vec) const noexcept {
        std::size_t seed = 0;
        for (auto const& el: vec) {
            seed = combineHash(std::hash<std::decay_t<decltype(el)> >{}(el), seed);
        }
        return seed;
    }
};
