#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gl::io {
// Parses a decimal token with at most `decimals` digits after the point into
// the exact integer sign * (integer part * 10^decimals + fractional digits),
// as a W. Never multiplies a parsed floating-point value. Accepts an optional
// sign, at least one digit, an optional point followed by 1..decimals digits.
// Throws std::invalid_argument for anything else: more decimals, exponents,
// empty digits, trailing text, or a magnitude above 2^digits(W) (where W stops
// holding every integer exactly). Meant as the parser of
// read_edge_list<W>(path, parse_weight).
template<std::floating_point W>
struct fixed_point_weight
{
  unsigned decimals = 2; // precondition (assert): 0..9

  W operator()(std::string_view token) const;

  // 10^decimals, exact.
  W scale() const;
};

template<std::floating_point W>
W fixed_point_weight<W>::scale() const
{
  assert(decimals <= 9 && "fixed_point_weight: decimals must be 0..9");
  long long s = 1;
  for (unsigned i = 0; i < decimals; ++i) {
    s *= 10;
  }
  return static_cast<W>(s);
}

template<std::floating_point W>
W fixed_point_weight<W>::operator()(std::string_view token) const
{
  assert(decimals <= 9 && "fixed_point_weight: decimals must be 0..9");

  // every integer up to 2^digits is exact in W
  constexpr long long limit = 1LL << std::numeric_limits<W>::digits;

  const auto fail = [&](const std::string& why) {
    throw std::invalid_argument(why + ": " + std::string(token));
  };
  const auto is_digit = [](char c) { return c >= '0' && c <= '9'; };

  long long acc = 0;
  // checked before every step, so acc never passes limit (no overflow)
  const auto push = [&](int digit) {
    if (acc > (limit - digit) / 10) {
      fail("weight too large to hold exactly");
    }
    acc = acc * 10 + digit;
  };

  std::size_t i = 0;
  bool negative = false;
  if (i < token.size() && (token[i] == '+' || token[i] == '-')) {
    negative = token[i] == '-';
    ++i;
  }

  std::size_t integer_digits = 0;
  for (; i < token.size() && is_digit(token[i]); ++i) {
    push(token[i] - '0');
    ++integer_digits;
  }
  if (integer_digits == 0) {
    fail("not a decimal weight");
  }

  unsigned fraction_digits = 0;
  if (i < token.size() && token[i] == '.') {
    ++i;
    for (; i < token.size() && is_digit(token[i]); ++i) {
      if (fraction_digits == decimals) {
        fail("weight has more than " + std::to_string(decimals) + " decimals");
      }
      push(token[i] - '0');
      ++fraction_digits;
    }
    if (fraction_digits == 0) {
      fail("not a decimal weight");
    }
  }
  if (i != token.size()) {
    fail("not a decimal weight");
  }

  for (; fraction_digits < decimals; ++fraction_digits) {
    push(0);
  }
  const W value = static_cast<W>(acc);
  return negative ? -value : value;
}
} // namespace gl::io
