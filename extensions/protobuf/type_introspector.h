// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_INTROSPECTOR_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

class ProtoTypeIntrospector : public TypeIntrospector {
 public:
  explicit ProtoTypeIntrospector(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool)
      : descriptor_pool_(descriptor_pool) {}

 protected:
  absl::StatusOr<absl::optional<TypeView>> FindTypeImpl(
      TypeFactory& type_factory, absl::string_view name,
      Type& scratch) const final;

  absl::StatusOr<absl::optional<StructTypeFieldView>>
  FindStructTypeFieldByNameImpl(TypeFactory& type_factory,
                                absl::string_view type, absl::string_view name,
                                StructTypeField& scratch) const final;

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() const {
    return descriptor_pool_;
  }

 private:
  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool_;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_INTROSPECTOR_H_
