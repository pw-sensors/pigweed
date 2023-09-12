// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <utility>

namespace pw {

namespace internal {

// Construct an instance of T in `p` through placement new, passing Args... to
// the constructor.
// This abstraction is here mostly for the gcc performance fix.
template <typename T, typename... Args>
void PlacementNew(void* p, Args&&... args) {
  new (p) T(std::forward<Args>(args)...);
}

}  // namespace internal

/// A paired-down `std::variant`-like with only two cases
template <typename A, typename B>
struct bivariant {
 public:
  bivariant() = delete;

  constexpr bivariant(const bivariant& other) : unused_(), tag_(other.tag_) {
    if (tag_ == TAG_A) {
      MakeA(other.value_a_);
    } else {
      MakeB(other.value_b_);
    }
  }

  constexpr bivariant(bivariant&& other) noexcept
      : unused_(), tag_(other.tag_) {
    if (tag_ == TAG_A) {
      MakeA(std::move(other.value_a_));
    } else {
      MakeB(std::move(other.value_b_));
    }
  }

  constexpr bivariant(A&& value_a)
      : value_a_(std::move(value_a)), tag_(TAG_A) {}
  constexpr bivariant(B&& value_b)
      : value_b_(std::move(value_b)), tag_(TAG_B) {}

  template <typename... Args>
  constexpr bivariant(std::in_place_type_t<A>, Args&&... args)
      : value_a_(std::forward<Args>(args)...), tag_(TAG_A) {}

  template <typename... Args>
  constexpr bivariant(std::in_place_type_t<B>, Args&&... args)
      : value_b_(std::forward<Args>(args)...), tag_(TAG_B) {}

  constexpr bivariant& operator=(const bivariant& other) {
    if (this == &other) {
      return *this;
    }
    if (other.tag_ == TAG_A) {
      return operator=(other.value_a_);
    } else {
      return operator=(other.value_b_);
    }
  }

  constexpr bivariant& operator=(const A& other) {
    if (tag_ == TAG_A) {
      value_a_ = other;
    } else {
      value_b_.~B();
      MakeA(other);
    }
    return *this;
  }

  constexpr bivariant& operator=(const B& other) {
    if (tag_ == TAG_B) {
      value_b_ = other;
    } else {
      value_a_.~A();
      MakeB(other);
    }
    return *this;
  }

  constexpr bivariant& operator=(bivariant&& other) {
    if (other.tag_ == TAG_A) {
      return operator=(std::move(other.value_a_));
    } else {
      return operator=(std::move(other.value_b_));
    }
  }

  constexpr bivariant& operator=(A&& other) {
    if (tag_ == TAG_A) {
      value_a_ = std::move(other);
    } else {
      value_b_.~B();
      MakeA(std::move(other));
    }
    return *this;
  }

  constexpr bivariant& operator=(B&& other) {
    if (tag_ == TAG_B) {
      value_b_ = std::move(other);
    } else {
      value_a_.~A();
      MakeB(std::move(other));
    }
    return *this;
  }

  constexpr bool is_a() const { return tag_ == TAG_A; }

  constexpr bool is_b() const { return tag_ == TAG_B; }

  constexpr A& value_a() { return value_a_; }

  constexpr const A& value_a() const { return value_a_; }

  constexpr B& value_b() { return value_b_; }

  constexpr const B& value_b() const { return value_b_; }

  ~bivariant() {
    if (tag_ == TAG_A) {
      value_a_.~A();
    } else {
      value_b_.~B();
    }
  }

 private:
  struct Empty {};
  union {
    Empty unused_;
    A value_a_;
    B value_b_;
  };
  enum { TAG_A, TAG_B } tag_;

  template <typename... Args>
  void MakeA(Args&&... args) {
    tag_ = TAG_A;
    internal::PlacementNew<A>(&value_a_, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MakeB(Args&&... args) {
    tag_ = TAG_B;
    internal::PlacementNew<B>(&value_b_, std::forward<Args>(args)...);
  }
};

}  // namespace pw
