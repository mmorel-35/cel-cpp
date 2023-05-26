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

#include "base/values/unknown_value.h"

#include <string>
#include <utility>

namespace cel {

CEL_INTERNAL_VALUE_IMPL(UnknownValue);

std::string UnknownValue::DebugString() const { return "*unknown*"; }

const AttributeSet& UnknownValue::attribute_set() const {
  return base_internal::Metadata::IsTrivial(*this)
             ? value_ptr_->unknown_attributes()
             : value_.unknown_attributes();
}

const FunctionResultSet& UnknownValue::function_result_set() const {
  return base_internal::Metadata::IsTrivial(*this)
             ? value_ptr_->unknown_function_results()
             : value_.unknown_function_results();
}

}  // namespace cel