// Copyright 2022 Google LLC
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

#include "base/types/map_type.h"

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/types/dyn_type.h"
#include "base/value_factory.h"
#include "base/values/map_value.h"
#include "base/values/map_value_builder.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(MapType);

absl::Span<const absl::string_view> MapType::aliases() const {
  static constexpr absl::string_view kAliases[] = {"google.protobuf.Struct"};
  if (key()->kind() == Kind::kString && value()->kind() == Kind::kDyn) {
    // Currently google.protobuf.Struct resolves to map<string, dyn>.
    return absl::MakeConstSpan(kAliases);
  }
  return absl::Span<const absl::string_view>();
}

std::string MapType::DebugString() const {
  return absl::StrCat(name(), "(", key()->DebugString(), ", ",
                      value()->DebugString(), ")");
}

const Handle<Type>& MapType::key() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyMapType&>(*this).key();
  }
  return static_cast<const base_internal::ModernMapType&>(*this).key();
}

const Handle<Type>& MapType::value() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyMapType&>(*this).value();
  }
  return static_cast<const base_internal::ModernMapType&>(*this).value();
}

namespace {

template <typename K>
absl::StatusOr<UniqueRef<MapValueBuilderInterface>> NewMapValueBuilderFor(
    ValueFactory& value_factory, Handle<MapType> type) {
  switch (type->value()->kind()) {
    case TypeKind::kBool:
      return MakeUnique<MapValueBuilder<K, BoolValue>>(
          value_factory.memory_manager(), value_factory, std::move(type));
    case TypeKind::kInt:
      return MakeUnique<MapValueBuilder<K, IntValue>>(
          value_factory.memory_manager(), value_factory, std::move(type));
    case TypeKind::kUint:
      return MakeUnique<MapValueBuilder<K, UintValue>>(
          value_factory.memory_manager(), value_factory, std::move(type));
    case TypeKind::kDouble:
      return MakeUnique<MapValueBuilder<K, DoubleValue>>(
          value_factory.memory_manager(), value_factory, std::move(type));
    case TypeKind::kDuration:
      return MakeUnique<MapValueBuilder<K, DurationValue>>(
          value_factory.memory_manager(), value_factory, std::move(type));
    case TypeKind::kTimestamp:
      return MakeUnique<MapValueBuilder<K, TimestampValue>>(
          value_factory.memory_manager(), value_factory, std::move(type));
    default:
      return MakeUnique<MapValueBuilder<K, Value>>(
          value_factory.memory_manager(), value_factory, std::move(type));
  }
}

}  // namespace

absl::StatusOr<UniqueRef<MapValueBuilderInterface>> MapType::NewValueBuilder(
    ValueFactory& value_factory) const {
  switch (key()->kind()) {
    case TypeKind::kBool:
      return NewMapValueBuilderFor<BoolValue>(value_factory,
                                              handle_from_this());
    case TypeKind::kInt:
      return NewMapValueBuilderFor<IntValue>(value_factory, handle_from_this());
    case TypeKind::kUint:
      return NewMapValueBuilderFor<UintValue>(value_factory,
                                              handle_from_this());
    default:
      return NewMapValueBuilderFor<Value>(value_factory, handle_from_this());
  }
}

namespace base_internal {

const Handle<Type>& LegacyMapType::key() const {
  return DynType::Get().As<Type>();
}

const Handle<Type>& LegacyMapType::value() const {
  return DynType::Get().As<Type>();
}

ModernMapType::ModernMapType(Handle<Type> key, Handle<Type> value)
    : MapType(),
      HeapData(kKind),
      key_(std::move(key)),
      value_(std::move(value)) {
  // Ensure `Type*` and `HeapData*` are not thunked.
  ABSL_ASSERT(reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
              reinterpret_cast<uintptr_t>(static_cast<HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel