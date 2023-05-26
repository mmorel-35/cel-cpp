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

#include "base/types/wrapper_type.h"

#include "absl/base/optimization.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace cel {

template class Handle<WrapperType>;
template class Handle<BoolWrapperType>;
template class Handle<BytesWrapperType>;
template class Handle<DoubleWrapperType>;
template class Handle<IntWrapperType>;
template class Handle<StringWrapperType>;
template class Handle<UintWrapperType>;

absl::string_view WrapperType::name() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->name();
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->name();
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->name();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->name();
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->name();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->name();
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

absl::Span<const absl::string_view> WrapperType::aliases() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->aliases();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->aliases();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->aliases();
    default:
      // The other wrappers do not have aliases.
      return absl::Span<const absl::string_view>();
  }
}

const Handle<Type>& WrapperType::wrapped() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->wrapped().As<Type>();
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

absl::Span<const absl::string_view> DoubleWrapperType::aliases() const {
  static constexpr absl::string_view kAliases[] = {
      "google.protobuf.FloatValue"};
  return absl::MakeConstSpan(kAliases);
}

absl::Span<const absl::string_view> IntWrapperType::aliases() const {
  static constexpr absl::string_view kAliases[] = {
      "google.protobuf.Int32Value"};
  return absl::MakeConstSpan(kAliases);
}

absl::Span<const absl::string_view> UintWrapperType::aliases() const {
  static constexpr absl::string_view kAliases[] = {
      "google.protobuf.UInt32Value"};
  return absl::MakeConstSpan(kAliases);
}

}  // namespace cel