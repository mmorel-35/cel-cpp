// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

// `MapValue` represents values of the primitive `map` type. `MapValueView`
// is a non-owning view of `MapValue`. `MapValueInterface` is the abstract
// base class of implementations. `MapValue` and `MapValueView` act as smart
// pointers to `MapValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/value_interface.h"
#include "common/value_kind.h"
#include "common/values/values.h"

namespace cel {

class Value;
class ValueView;
class ListValue;
class MapValueInterface;
class MapValue;
class MapValueView;
class MapValueBuilderInterface;
template <typename K, typename V>
class MapValueBuilder;
class TypeFactory;

class MapValueInterface : public ValueInterface {
 public:
  using alternative_type = MapValue;
  using view_alternative_type = MapValueView;

  static constexpr ValueKind kKind = ValueKind::kMap;

  // Checks whether the given key is a valid type that can be used as a map key.
  static absl::Status CheckKey(ValueView key);

  ValueKind kind() const final { return kKind; }

  MapTypeView type() const { return Cast<MapTypeView>(get_type()); }

  // Returns `true` if this map contains no entries, `false` otherwise.
  virtual bool IsEmpty() const { return Size() == 0; }

  // Returns the number of entries in this map.
  virtual size_t Size() const = 0;

  // Lookup the value associated with the given key, returning a view of the
  // value. If the implementation is not able to directly return a view, the
  // result is stored in `scratch` and the returned view is that of `scratch`.
  absl::StatusOr<ValueView> Get(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // Lookup the value associated with the given key, returning a view of the
  // value and a bool indicating whether it exists. If the implementation is not
  // able to directly return a view, the result is stored in `scratch` and the
  // returned view is that of `scratch`.
  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // Checks whether the given key is present in the map.
  absl::StatusOr<ValueView> Has(ValueView key) const;

  // Returns a new list value whose elements are the keys of this map.
  virtual absl::StatusOr<ListValue> ListKeys(
      TypeFactory& type_factory) const = 0;

  // Callback used by `ForEach`. The first argument is the key and the second is
  // the value. Returning a non-OK status causes `ForEach` to return that
  // status. Returning `true` causes `ForEach` to continue to the next entry`.
  // Returning `false` causes `ForEach` to return an OK status without
  // processing additional entries.
  using ForEachCallback =
      absl::FunctionRef<absl::StatusOr<bool>(ValueView, ValueView)>;

  // Iterates over the entries in the map, invoking `callback` for each. See the
  // comment on `ForEachCallback` for details.
  virtual absl::Status ForEach(ForEachCallback callback) const;

  // By default, implementations do not guarantee any iteration order. Unless
  // specified otherwise, assume the iteration order is random.
  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator()
      const = 0;

 private:
  // Called by `Find` after performing various argument checks.
  virtual absl::StatusOr<absl::optional<ValueView>> FindImpl(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;

  // Called by `Has` after performing various argument checks.
  virtual absl::StatusOr<bool> HasImpl(ValueView key) const = 0;
};

class MapValue {
 public:
  using interface_type = MapValueInterface;
  using view_alternative_type = MapValueView;

  static constexpr ValueKind kKind = MapValueInterface::kKind;

  static absl::Status CheckKey(ValueView key);

  explicit MapValue(MapValueView value);

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(Shared<const MapValueInterface> interface)
      : interface_(std::move(interface)) {}

  // By default, this creates an empty map whose type is `map(dyn, dyn)`. Unless
  // you can help it, you should use a more specific typed map value.
  MapValue();
  MapValue(const MapValue&) = default;
  MapValue(MapValue&&) = default;
  MapValue& operator=(const MapValue&) = default;
  MapValue& operator=(MapValue&&) = default;

  ValueKind kind() const { return interface_->kind(); }

  MapTypeView type() const { return interface_->type(); }

  std::string DebugString() const { return interface_->DebugString(); }

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Get(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Has(ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ListValue> ListKeys(TypeFactory& type_factory) const;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(ForEachCallback callback) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const;

  void swap(MapValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend class MapValueView;
  friend struct NativeTypeTraits<MapValue>;

  Shared<const MapValueInterface> interface_;
};

inline void swap(MapValue& lhs, MapValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const MapValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<MapValue> final {
  static NativeTypeId Id(const MapValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const MapValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<MapValue, T>>,
                               std::is_base_of<MapValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<MapValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<MapValue>::SkipDestructor(type);
  }
};

// MapValue -> MapValueFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<MapValue, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<MapValue, To>>,
        std::is_base_of<MapValue, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `MapValue`, and `To` has the
    // same size and alignment as `MapValue`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

class MapValueView {
 public:
  using interface_type = MapValueInterface;
  using alternative_type = MapValue;

  static constexpr ValueKind kKind = MapValue::kKind;

  static absl::Status CheckKey(ValueView key);

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValueView(const MapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : interface_(value.interface_) {}

  // By default, this creates an empty map whose type is `map(dyn, dyn)`. Unless
  // you can help it, you should use a more specific typed map value.
  MapValueView();
  MapValueView(const MapValueView&) = default;
  MapValueView(MapValueView&&) = default;
  MapValueView& operator=(const MapValueView&) = default;
  MapValueView& operator=(MapValueView&&) = default;

  ValueKind kind() const { return interface_->kind(); }

  MapTypeView type() const { return interface_->type(); }

  std::string DebugString() const { return interface_->DebugString(); }

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Get(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueView key, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Has(ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ListValue> ListKeys(TypeFactory& type_factory) const;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(ForEachCallback callback) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const;

  void swap(MapValueView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend class MapValue;
  friend struct NativeTypeTraits<MapValueView>;

  SharedView<const MapValueInterface> interface_;
};

inline void swap(MapValueView& lhs, MapValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, MapValueView type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<MapValueView> final {
  static NativeTypeId Id(MapValueView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<MapValueView, T>>,
                               std::is_base_of<MapValueView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<MapValueView>::Id(type);
  }
};

inline MapValue::MapValue(MapValueView value) : interface_(value.interface_) {}

// MapValueView -> MapValueViewFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<MapValueView, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<MapValueView, To>>,
        std::is_base_of<MapValueView, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `OpaqueType`, and `To` has the
    // same size and alignment as `OpaqueType`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

namespace common_internal {
template <typename T>
struct MapValueKeyHash;
template <typename T>
struct MapValueKeyEqualTo;
template <typename K, typename V>
using ValueFlatHashMapFor =
    absl::flat_hash_map<K, V, MapValueKeyHash<K>, MapValueKeyEqualTo<K>>;
}  // namespace common_internal

class MapValueBuilderInterface {
 public:
  virtual ~MapValueBuilderInterface() = default;

  virtual void Put(Value key, Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity) {}

  virtual MapValue Build() && = 0;
};

template <typename K, typename V>
class MapValueBuilder final : public MapValueBuilderInterface {
 public:
  using key_view_type = std::decay_t<decltype(std::declval<K>().type())>;
  using key_type = typename key_view_type::alternative_type;
  using value_view_type = std::decay_t<decltype(std::declval<V>().type())>;
  using value_type = typename value_view_type::alternative_type;

  static_assert(common_internal::IsValueAlternativeV<K>,
                "K must be Value or one of the Value alternatives");
  static_assert(common_internal::IsValueAlternativeV<V>,
                "V must be Value or one of the Value alternatives");

  MapValueBuilder(TypeFactory& type_factory ABSL_ATTRIBUTE_LIFETIME_BOUND,
                  key_view_type key, value_view_type value)
      : MapValueBuilder(type_factory.memory_manager(),
                        type_factory.CreateMapType(key, value)) {}

  MapValueBuilder(MemoryManagerRef memory_manager, MapType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  MapValueBuilder(const MapValueBuilder&) = delete;
  MapValueBuilder(MapValueBuilder&&) = delete;
  MapValueBuilder& operator=(const MapValueBuilder&) = delete;
  MapValueBuilder& operator=(MapValueBuilder&&) = delete;

  void Put(Value key, Value value) override;

  void Put(K key, V value);

  void Put(K key, Value value);

  void Put(Value key, V value);

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  void Reserve(size_t capacity) override { entries_.reserve(capacity); }

  MapValue Build() && override;

 private:
  MemoryManagerRef memory_manager_;
  MapType type_;
  common_internal::ValueFlatHashMapFor<K, V> entries_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_