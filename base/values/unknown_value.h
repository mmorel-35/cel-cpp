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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_UNKNOWN_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_UNKNOWN_VALUE_H_

#include <string>

#include "absl/hash/hash.h"
#include "base/types/unknown_type.h"
#include "base/value.h"

namespace cel {

class UnknownValue final : public Value, public base_internal::HeapData {
 public:
  static constexpr Kind kKind = UnknownType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  const Persistent<const UnknownType>& type() const {
    return UnknownType::Get();
  }

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Value& other) const;

 private:
  friend class cel::MemoryManager;
  friend class ValueFactory;

  UnknownValue();
};

CEL_INTERNAL_VALUE_DECL(UnknownValue);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_UNKNOWN_VALUE_H_
