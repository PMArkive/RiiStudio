#pragma once

#include <algorithm>
#include <ranges>
#include <vector>

namespace rsl {

template <typename T> static bool RangeIsHomogenous(const T& range) {
  return std::adjacent_find(range.begin(), range.end(),
                            std::not_equal_to<>()) == range.end();
}

template <typename T> struct Collector {};
template <size_t N, typename T> struct ArrayCollector {};

// A-la C#
template <typename T = void> static auto ToList() { return Collector<T>{}; }
template <size_t N, typename T = void> static auto ToArray() {
  return ArrayCollector<N, T>{};
}

template <typename T>
using element_type_t =
    std::remove_reference_t<decltype(*std::begin(std::declval<T&>()))>;

} // namespace rsl

template <typename R, typename T>
static auto operator|(R&& range, rsl::Collector<T>&&) {
  // Value of each item in the range
  using FallbackValueT = rsl::element_type_t<R>;
  // If the typename T overload is picked, use it; otherwise default
  constexpr bool IsCustomValueT = !std::is_void_v<T>;
  using ValueT = std::conditional_t<IsCustomValueT, T, FallbackValueT>;
  return std::vector<std::remove_cvref_t<ValueT>>(range.begin(), range.end());
}
template <typename R, size_t N, typename T>
static auto operator|(R&& range, rsl::ArrayCollector<N, T>&&) {
  // Value of each item in the range
  using FallbackValueT = rsl::element_type_t<R>;
  // If the typename T overload is picked, use it; otherwise default
  constexpr bool IsCustomValueT = !std::is_void_v<T>;
  using ValueT = std::conditional_t<IsCustomValueT, T, FallbackValueT>;

  auto size = std::ranges::size(range);
  std::array<std::remove_cvref_t<ValueT>, N> arr;
  std::copy_n(range.begin(), std::min(size, arr.size()), arr.begin());
  std::remove_cvref_t<ValueT> dv{};
  std::fill(arr.begin() + std::min(size, arr.size()), arr.end(), dv);
  return arr;
}

namespace rsl {

// Adapted from https://stackoverflow.com/a/28769484
template <typename T, typename TIter = decltype(std::begin(std::declval<T>())),
          typename = decltype(std::end(std::declval<T>()))>
constexpr static auto enumerate(T&& iterable) {
  struct iterator {
    using self_type [[maybe_unused]] = iterator;
    using value_type [[maybe_unused]] =
        std::tuple<size_t, typename TIter::value_type>;
    using reference [[maybe_unused]] = value_type&;
    using pointer [[maybe_unused]] = value_type*;
    using iterator_category [[maybe_unused]] = std::forward_iterator_tag;
    using difference_type [[maybe_unused]] = std::ptrdiff_t;
    size_t i;
    TIter iter;
    bool operator==(const iterator& other) const { return iter == other.iter; }
    bool operator!=(const iterator& other) const { return iter != other.iter; }
    iterator& operator++() {
      ++i;
      ++iter;
      return *this;
    }
    iterator operator++(int) {
      iterator it = *this;
      ++it;
      return it;
    }
    value_type operator*() const { return std::tie(i, *iter); }
  };
  struct iterable_wrapper {
    T iterable;
    auto begin() { return iterator{0, std::begin(iterable)}; }
    auto end() { return iterator{0, std::end(iterable)}; }
  };
  return iterable_wrapper{std::forward<T>(iterable)};
}

struct Enumerator {};

constexpr static Enumerator enumerate() { return {}; }

} // namespace rsl

static auto operator|(auto&& range, rsl::Enumerator&&) {
  return rsl::enumerate(std::move(range));
}

namespace rsl {

// Adapted from
// https://www.techiedelight.com/implode-a-vector-of-strings-into-a-comma-separated-string-in-cpp/
//
// - Take strings by const ref, making it actually compile
// - Operate on any range, not just std::vector<std::string>
//
// Until we get ranges printing in the STL
constexpr inline std::string join(auto&& strings, std::string delim) {
  std::string s;
  return std::accumulate(
      strings.begin(), strings.end(), s,
      [&delim](const std::string& x, const auto& y) -> std::string {
        if (x.empty()) {
          return std::string(y);
        }
        return x + delim + std::string(y);
      });
}

} // namespace rsl
