// MPark.Variant
//
// Copyright Michael Park, 2015-2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// GPU compatibility corrections are © 2025. Triad National Security,
// LLC under U.S. Government contract 89233218CNA000001 for Los Alamos
// National Laboratory (LANL), which is operated by Triad National
// Security, LLC for the U.S. Department of Energy/National Nuclear
// Security Administration.


#ifndef _PORTS_OF_CALL_VARIANT_IN_PLACE_HPP_
#define _PORTS_OF_CALL_VARIANT_IN_PLACE_HPP_

#include <cstddef>

#include "config.hpp"

namespace PortsOfCall {

  struct in_place_t { explicit in_place_t() = default; };

  template <std::size_t I>
  struct in_place_index_t { explicit in_place_index_t() = default; };

  template <typename T>
  struct in_place_type_t { explicit in_place_type_t() = default; };

#ifdef MPARK_VARIABLE_TEMPLATES
  constexpr in_place_t in_place{};

  template <std::size_t I> constexpr in_place_index_t<I> in_place_index{};

  template <typename T> constexpr in_place_type_t<T> in_place_type{};
#endif

}  // namespace PortsOfCall

#endif  // _PORTS_OF_CALL_VARIANT_IN_PLACE_HPP_
