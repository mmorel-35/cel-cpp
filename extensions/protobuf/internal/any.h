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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::Status SetAny(google::protobuf::Message& message, absl::string_view type_url,
                    const absl::Cord& value);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_
