#pragma once

#include <concepts>
/* concepts are named yes/no questions about a type - answered at compile-time*/
#include <type_traits> // for bool_constant

namespace gl {
/* tag type - never instantiated (carries info at compile time) */
struct undirected
{
  static constexpr bool is_directed = false;
};

/* tag type - never instantiated (carries info at compile time) */
struct directed
{
  static constexpr bool is_directed = true;
};

template<typename T>
concept direction_policy = requires { // lists expressions that must compile.
                                      // Evaluates to either true or false
  { T::is_directed } -> std::convertible_to<bool>;

  // std::bool_constant<X>, from <type_traits>
  // only exists if X is a compile-time constant
  // Here -> forces T::is_directed to be constexpr
  typename std::bool_constant<T::is_directed>;
};

struct unweighted
{
  static constexpr bool is_weighted = false;
};

template<std::floating_point W =
           double> // all float types (i.e.: float & double)
struct weighted
{
  static constexpr bool is_weighted = true;
  using weight_type = W;
};

template<typename T>
concept has_weight_flag = requires {
  { T::is_weighted } -> std::convertible_to<bool>;

  // std::bool_constant<X>, from <type_traits>
  // only exists if X is a compile-time constant
  // Here -> forces T::is_weighted to be constexpr
  typename std::bool_constant<T::is_weighted>;
};

template<typename T>
concept has_weight_type = requires { typename T::weight_type; };

template<typename T>
concept weight_policy =
  has_weight_flag<T> && (!T::is_weighted || has_weight_type<T>);
}
