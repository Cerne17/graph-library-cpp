#pragma once

#include <concepts>
/* concepts are named yes/no questions about a type - answered at compile-time*/
#include <type_traits> // for bool_constant

/* tag type - never instantiated (carries info at compile time) */
struct Undirected
{
  static constexpr bool directed = false;
};

/* tag type - never instantiated (carries info at compile time) */
struct Directed
{
  static constexpr bool directed = true;
};

template<typename T>
concept DirectionPolicy = requires { // lists expressions that must compile.
                                     // Evaluates to either true or false
  { T::directed } -> std::convertible_to<bool>;

  // std::bool_constant<X>, from <type_traits>
  // only exists if X is a compile-time constant
  // Here -> forces T::directed to be constexpr
  typename std::bool_constant<T::directed>;
};

struct Unweighted
{
  static constexpr bool weighted = false;
};

template<std::floating_point W =
           double> // all float types (i.e.: float & double)
struct Weighted
{
  static constexpr bool weighted = true;
  using weight_type = W;
};

template<typename T>
concept HasWeightedFlag = requires {
  { T::weighted } -> std::convertible_to<bool>;

  // std::bool_constant<X>, from <type_traits>
  // only exists if X is a compile-time constant
  // Here -> forces T::weighted to be constexpr
  typename std::bool_constant<T::weighted>;
};

template<typename T>
concept HasWeightType = requires { typename T::weight_type; };

template<typename T>
concept WeightPolicy = HasWeightedFlag<T> && (!T::weighted || HasWeightType<T>);
