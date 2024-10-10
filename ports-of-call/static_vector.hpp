#ifndef _PORTS_OF_CALL_STATIC_VECTOR_HPP_
#define _PORTS_OF_CALL_STATIC_VECTOR_HPP_

#include "ports-of-call/portability.hpp"

#include <cassert>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace PortsOfCall {

/* The static_vector has an interface modeled after std::vector, but it uses
 * statically-allocated memory to (a) avoid dynamic memory allocation, deallocation, or
 * reallocation; and (b) ensure that the data is contained entirely inside the
 * static_vector so that this type can be memcopied to a CPU.  We had an initial
 * implementation that provided additional features, but was not portable to GPUs.  This
 * implementation is simpler and less feature-rich, but works on GPUs.  Tests have been
 * added to the test suite to ensure that this is true and to enforce that it continues to
 * be true.  Features that are missing:
 * -- The insert, emplace, and erase methods from std::vector were included in the initial
 *    implementation, but have not been included in this implementation.  These methods
 * could likely be added to this implementation if needed.
 * -- The initial implementation could handle a size of N = 0, while this implementation
 * will fail to compile if the size is zero.  Support for zero-size static_vector could
 * likely be added to this implementation if needed.
 * -- The initial implementation propagated triviality.  If the type held by the
 * static_vector (Element) is trivially-destructible, then the initial implementation of
 * static_vector would also be trivially-destructible.  We have not yet been able to
 * reproduce this behavior in a GPU-compatible way, so the new implementation of
 * static_vector is never trivially-destructible.
 * -- The initial implementation could adapt to holding types that are not copyable and/or
 * not movable.  We have not yet been able to reproduce this behavior in a GPU-compatible
 * way, so the new implementation assumes that objects of type Element are both copyable
 * and movable.
 * -- The initial implementation was able to perform some optimizations on triviality of
 * the Element type.  We have not yet been able to reproduce this behavior in a
 * GPU-compatible way, so the new implementation may not perform as fast in some cases.
 */

template <typename Element, std::size_t N>
class static_vector {
 public:
  // iterator stolen from CJ's static_vector
  // ----------------------------------------------------
  template <typename Element_>
  class iterator_type {
    friend class static_vector<Element, N>;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = Element_;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

    using const_iterator_type = iterator_type<Element_ const>;

    PORTABLE_FUNCTION constexpr iterator_type() : it_{nullptr} {}

    PORTABLE_FUNCTION constexpr iterator_type(Element_ *it) : it_{it} {}

    // const iterator from non-const iterator
    template <typename U = value_type,
              std::enable_if_t<std::is_const<U>::value> * = nullptr>
    PORTABLE_FUNCTION constexpr iterator_type(iterator_type<std::remove_const_t<U>> it)
        : it_{it.it_} {}

   private:
    Element_ *it_;

    // non-const iterator from const iterator...generally dangerous
    // (used for convinience in static_vector implementation)
    template <typename U = value_type,
              std::enable_if_t<not std::is_const<U>::value> * = nullptr>
    PORTABLE_FUNCTION constexpr iterator_type(iterator_type<std::add_const_t<U>> it)
        : it_{const_cast<U *>(it.it_)} {}

   public:
    PORTABLE_FUNCTION constexpr iterator_type &operator++() {
      ++(this->it_);
      return *this;
    }

    PORTABLE_FUNCTION constexpr iterator_type operator++(int) const {
      iterator_type temp{*this};
      ++temp;
      return temp;
    }

    PORTABLE_FUNCTION constexpr iterator_type &operator--() {
      --(this->it_);
      return *this;
    }

    PORTABLE_FUNCTION constexpr iterator_type operator--(int) const {
      iterator_type temp{*this};
      --temp;
      return temp;
    }

    PORTABLE_FUNCTION constexpr iterator_type &operator+=(difference_type m) {
      this->it_ += m;
      return *this;
    }

    PORTABLE_FUNCTION constexpr iterator_type &operator-=(difference_type m) {
      return *this += -m;
    }

    PORTABLE_FUNCTION constexpr iterator_type operator+(difference_type m) const {
      iterator_type temp{*this};
      return temp += m;
    }

    PORTABLE_FUNCTION constexpr iterator_type operator-(difference_type m) const {
      iterator_type temp{*this};
      return temp -= m;
    }

    PORTABLE_FUNCTION constexpr difference_type
    operator-(const_iterator_type other) const {
      return this->it_ - other.it_;
    }

    PORTABLE_FUNCTION constexpr bool operator==(const_iterator_type other) const {
      return this->it_ == other.it_;
    }

    PORTABLE_FUNCTION constexpr bool operator!=(const_iterator_type other) const {
      return not(this->it_ == other.it_);
    }

    PORTABLE_FUNCTION constexpr bool operator<(const_iterator_type other) const {
      return this->it_ < other.it_;
    }

    PORTABLE_FUNCTION constexpr bool operator>(const_iterator_type other) const {
      return other < *this;
    }

    PORTABLE_FUNCTION constexpr bool operator>=(const_iterator_type other) const {
      return not(*this < other);
    }

    PORTABLE_FUNCTION constexpr bool operator<=(const_iterator_type other) const {
      return not(*this > other);
    }

    PORTABLE_FUNCTION constexpr reference operator*() const { return *(this->it_); }

    PORTABLE_FUNCTION constexpr pointer operator->() const { return this->it_; }

    PORTABLE_FUNCTION constexpr reference operator[](difference_type n) const {
      return *(this->it_ + n);
    }

    template <typename IntT, std::enable_if_t<std::is_integral<IntT>::value> * = nullptr>
    PORTABLE_FUNCTION friend constexpr void advance(iterator_type &it, IntT n) {
      it += n;
    }

    PORTABLE_FUNCTION friend constexpr iterator_type next(iterator_type it,
                                                          difference_type n) {
      advance(it, n);
      return it;
    }

    PORTABLE_FUNCTION friend constexpr iterator_type prev(iterator_type it,
                                                          difference_type n) {
      advance(it, -n);
      return it;
    }

    PORTABLE_FUNCTION friend constexpr iterator_type::difference_type
    distance(iterator_type first, iterator_type last) {
      return last - first;
    }
  };
  // end of iterator
  // ----------------------------------------------------------------------------

 public:
  using value_type = Element;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = Element &;
  using const_reference = Element const &;
  using pointer = Element *;
  using const_pointer = Element const *;
  using iterator = iterator_type<Element>;
  using const_iterator = iterator_type<Element const>;

 private:
  size_type count_ = 0;
  using byte = unsigned char;
  byte storage_[sizeof(Element) * N];
  PORTABLE_FUNCTION constexpr pointer ptr() {
    return reinterpret_cast<pointer>(storage_);
  }
  PORTABLE_FUNCTION constexpr pointer ptr(size_type const i) { return ptr() + i; }
  PORTABLE_FUNCTION constexpr reference ref() { return *ptr(); }
  PORTABLE_FUNCTION constexpr reference ref(size_type const i) { return *ptr(i); }
  PORTABLE_FUNCTION constexpr const_pointer ptr() const {
    return reinterpret_cast<const_pointer>(storage_);
  }
  PORTABLE_FUNCTION constexpr const_pointer ptr(size_type const i) const {
    return ptr() + i;
  }
  PORTABLE_FUNCTION constexpr const_reference ref() const { return *ptr(); }
  PORTABLE_FUNCTION constexpr const_reference ref(size_type const i) const {
    return *ptr(i);
  }

 public:
  // constructor
  // --------------------------------------------------------------------------------
  // PORTABLE_FUNCTION constexpr static_vector() = default;
  PORTABLE_FUNCTION constexpr static_vector() {}

  PORTABLE_FUNCTION constexpr static_vector(std::initializer_list<Element> const &list) {
    assert(list.size() <= N);
    for (auto &element : list) {
      push_back(element);
    }
  }

  PORTABLE_FUNCTION constexpr static_vector(std::initializer_list<Element> &&list) {
    assert(list.size() <= N);
    for (auto &&element : list) {
      push_back(element);
    }
  }

  // copy constructor
  // ---------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr static_vector(static_vector const &other) {
    clear();
    for (auto const &element : other) {
      push_back(element);
    }
  }

  // move constructor
  // ---------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr static_vector(static_vector &&other) noexcept {
    clear();
    for (auto &&element : other) {
      push_back(element);
    }
  }

  // copy assignment
  // ----------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr static_vector &operator=(static_vector const &other) {
    clear();
    for (auto const &element : other) {
      push_back(element);
    }
    return *this;
  }

  // move assignment
  // ----------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr static_vector &operator=(static_vector &&other) noexcept {
    clear();
    for (auto &&element : other) {
      push_back(element);
    }
    return *this;
  }

  // destructor
  // ---------------------------------------------------------------------------------
  PORTABLE_FUNCTION ~static_vector() { clear(); }

  // operator[]
  // ---------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr reference operator[](size_type const i) & { return ref(i); }

  PORTABLE_FUNCTION constexpr const_reference operator[](size_type const i) const & {
    return ref(i);
  }

  PORTABLE_FUNCTION constexpr Element &&operator[](size_type const i) && {
    return std::move(ref(i));
  }

  PORTABLE_FUNCTION constexpr Element const &&operator[](size_type const i) const && {
    return std::move(ref(i));
  }

  // front
  // --------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr reference front() & { return ref(); }

  PORTABLE_FUNCTION constexpr const_reference front() const & { return ref(); }

  PORTABLE_FUNCTION constexpr Element &&front() && { return ref(); }

  PORTABLE_FUNCTION constexpr Element const &&front() const && { return ref(); }

  // back
  // ---------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr reference back() & { return ref(count_ - 1); }

  PORTABLE_FUNCTION constexpr const_reference back() const & { return ref(count_ - 1); }

  PORTABLE_FUNCTION constexpr Element &&back() && { return ref(count_ - 1); }

  PORTABLE_FUNCTION constexpr Element const &&back() const && { return ref(count_ - 1); }

  // data
  // ---------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr pointer data() { return ptr(); }

  PORTABLE_FUNCTION constexpr const_pointer data() const { return ptr(); }

  // begin
  // --------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr iterator begin() { return ptr(); }

  PORTABLE_FUNCTION constexpr const_iterator begin() const { return ptr(); }

  PORTABLE_FUNCTION constexpr const_iterator cbegin() const { return ptr(); }

  // end
  // ----------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr iterator end() { return next(begin(), count_); }

  PORTABLE_FUNCTION constexpr const_iterator end() const { return next(begin(), count_); }

  PORTABLE_FUNCTION constexpr const_iterator cend() const {
    return next(cbegin(), count_);
  }

  // empty
  // --------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr bool empty() const { return count_ == 0; }

  // size
  // ---------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr size_type size() const { return count_; }

  // max_size
  // -----------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr size_type max_size() const { return N; }

  // capacity
  // -----------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr size_type capacity() const { return max_size(); }

  // clear
  // --------------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr void clear() noexcept {
    for (size_type i = 0; i < count_; ++i) {
      ptr(i)->~value_type();
    }
    count_ = 0;
  }

  // push_back
  // ----------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr void push_back(value_type const &value) {
    assert(count_ < N);
    new (ptr(count_)) value_type{value};
    ++count_;
  }

  PORTABLE_FUNCTION constexpr void push_back(value_type &&value) {
    assert(count_ < N);
    new (ptr(count_)) value_type{std::move(value)};
    ++count_;
  }

  // emplace_back
  // -------------------------------------------------------------------------------
  template <typename... Args>
  PORTABLE_FUNCTION constexpr reference emplace_back(Args &&...args) {
    assert(count_ < N);
    new (ptr(count_)) value_type(std::forward<Args>(args)...);
    ++count_;
    return back();
  }

  // pop_back
  // -----------------------------------------------------------------------------------
  PORTABLE_FUNCTION constexpr void pop_back() {
    if (count_ > 0) {
      ptr(count_ - 1)->~value_type();
      --count_;
    }
  }

  // free function begin
  // ------------------------------------------------------------------------
  PORTABLE_FUNCTION friend constexpr iterator begin(static_vector &sv) {
    return sv.begin();
  }

  PORTABLE_FUNCTION friend constexpr const_iterator begin(static_vector const &sv) {
    return sv.begin();
  }

  PORTABLE_FUNCTION friend constexpr const_iterator cbegin(static_vector const &sv) {
    return sv.cbegin();
  }

  // free function end
  // --------------------------------------------------------------------------
  PORTABLE_FUNCTION friend constexpr iterator end(static_vector &sv) { return sv.end(); }

  PORTABLE_FUNCTION friend constexpr const_iterator end(static_vector const &sv) {
    return sv.end();
  }

  PORTABLE_FUNCTION friend constexpr const_iterator cend(static_vector const &sv) {
    return sv.cend();
  }
};

} // namespace PortsOfCall

#endif // ifndef _PORTS_OF_CALL_STATIC_VECTOR_HPP_
