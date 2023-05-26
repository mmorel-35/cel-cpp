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

#include "base/value.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "base/values/optional_value.h"
#include "internal/benchmark.h"
#include "internal/strings.h"
#include "internal/testing.h"
#include "internal/time.h"

namespace cel {

namespace {

using testing::Eq;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

enum class TestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

class TestEnumType final : public EnumType {
 public:
  using EnumType::EnumType;

  absl::string_view name() const override { return "test_enum.TestEnum"; }

  size_t constant_count() const override { return 2; }

  absl::StatusOr<UniqueRef<ConstantIterator>> NewConstantIterator(
      MemoryManager& memory_manager) const override {
    return absl::UnimplementedError(
        "EnumType::NewConstantIterator is unimplemented");
  }

 protected:
  absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const override {
    if (name == "VALUE1") {
      return Constant(MakeConstantId(1), "VALUE1", 1);
    }
    if (name == "VALUE2") {
      return Constant(MakeConstantId(2), "VALUE2", 2);
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const override {
    switch (number) {
      case 1:
        return Constant(MakeConstantId(1), "VALUE1", 1);
      case 2:
        return Constant(MakeConstantId(2), "VALUE2", 2);
      default:
        return absl::nullopt;
    }
  }

 private:
  CEL_DECLARE_ENUM_TYPE(TestEnumType);
};

CEL_IMPLEMENT_ENUM_TYPE(TestEnumType);

struct TestStruct final {
  bool bool_field = false;
  int64_t int_field = 0;
  uint64_t uint_field = 0;
  double double_field = 0.0;
};

bool operator==(const TestStruct& lhs, const TestStruct& rhs) {
  return lhs.bool_field == rhs.bool_field && lhs.int_field == rhs.int_field &&
         lhs.uint_field == rhs.uint_field &&
         lhs.double_field == rhs.double_field;
}

template <typename H>
H AbslHashValue(H state, const TestStruct& test_struct) {
  return H::combine(std::move(state), test_struct.bool_field,
                    test_struct.int_field, test_struct.uint_field,
                    test_struct.double_field);
}

class TestStructValue final : public CEL_STRUCT_VALUE_CLASS {
 public:
  explicit TestStructValue(const Handle<StructType>& type)
      : TestStructValue(type, TestStruct{}) {}

  explicit TestStructValue(const Handle<StructType>& type, TestStruct value)
      : CEL_STRUCT_VALUE_CLASS(type), value_(std::move(value)) {}

  std::string DebugString() const override {
    return absl::StrCat("bool_field: ", value().bool_field,
                        " int_field: ", value().int_field,
                        " uint_field: ", value().uint_field,
                        " double_field: ", value().double_field);
  }

  const TestStruct& value() const { return value_; }

  absl::StatusOr<Handle<Value>> GetFieldByName(
      const GetFieldContext& context, absl::string_view name) const override {
    if (name == "bool_field") {
      return context.value_factory().CreateBoolValue(value().bool_field);
    } else if (name == "int_field") {
      return context.value_factory().CreateIntValue(value().int_field);
    } else if (name == "uint_field") {
      return context.value_factory().CreateUintValue(value().uint_field);
    } else if (name == "double_field") {
      return context.value_factory().CreateDoubleValue(value().double_field);
    }
    return absl::NotFoundError("");
  }

  absl::StatusOr<Handle<Value>> GetFieldByNumber(
      const GetFieldContext& context, int64_t number) const override {
    switch (number) {
      case 0:
        return context.value_factory().CreateBoolValue(value().bool_field);
      case 1:
        return context.value_factory().CreateIntValue(value().int_field);
      case 2:
        return context.value_factory().CreateUintValue(value().uint_field);
      case 3:
        return context.value_factory().CreateDoubleValue(value().double_field);
      default:
        return absl::NotFoundError("");
    }
  }

  absl::StatusOr<bool> HasFieldByName(const HasFieldContext& context,
                                      absl::string_view name) const override {
    if (name == "bool_field") {
      return true;
    } else if (name == "int_field") {
      return true;
    } else if (name == "uint_field") {
      return true;
    } else if (name == "double_field") {
      return true;
    }
    return absl::NotFoundError("");
  }

  absl::StatusOr<bool> HasFieldByNumber(const HasFieldContext& context,
                                        int64_t number) const override {
    switch (number) {
      case 0:
        return true;
      case 1:
        return true;
      case 2:
        return true;
      case 3:
        return true;
      default:
        return absl::NotFoundError("");
    }
  }

  size_t field_count() const override { return 4; }

  absl::StatusOr<UniqueRef<FieldIterator>> NewFieldIterator(
      MemoryManager& memory_manager) const override {
    return absl::UnimplementedError(
        "StructValue::NewFieldIterator() is unimplemented");
  }

 private:
  TestStruct value_;

  CEL_DECLARE_STRUCT_VALUE(TestStructValue);
};

CEL_IMPLEMENT_STRUCT_VALUE(TestStructValue);

class TestStructType final : public CEL_STRUCT_TYPE_CLASS {
 public:
  absl::string_view name() const override { return "test_struct.TestStruct"; }

  size_t field_count() const override { return 4; }

  absl::StatusOr<UniqueRef<FieldIterator>> NewFieldIterator(
      MemoryManager& memory_manager) const override {
    return absl::UnimplementedError(
        "StructType::NewFieldIterator() is unimplemented");
  }

 protected:
  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const override {
    if (name == "bool_field") {
      return Field(MakeFieldId(0), "bool_field", 0,
                   type_manager.type_factory().GetBoolType());
    } else if (name == "int_field") {
      return Field(MakeFieldId(1), "int_field", 1,
                   type_manager.type_factory().GetIntType());
    } else if (name == "uint_field") {
      return Field(MakeFieldId(2), "uint_field", 2,
                   type_manager.type_factory().GetUintType());
    } else if (name == "double_field") {
      return Field(MakeFieldId(3), "double_field", 3,
                   type_manager.type_factory().GetDoubleType());
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager, int64_t number) const override {
    switch (number) {
      case 0:
        return Field(MakeFieldId(0), "bool_field", 0,
                     type_manager.type_factory().GetBoolType());
      case 1:
        return Field(MakeFieldId(1), "int_field", 1,
                     type_manager.type_factory().GetIntType());
      case 2:
        return Field(MakeFieldId(2), "uint_field", 2,
                     type_manager.type_factory().GetUintType());
      case 3:
        return Field(MakeFieldId(3), "double_field", 3,
                     type_manager.type_factory().GetDoubleType());
      default:
        return absl::nullopt;
    }
  }

 private:
  CEL_DECLARE_STRUCT_TYPE(TestStructType);
};

CEL_IMPLEMENT_STRUCT_TYPE(TestStructType);

class TestListValue final : public CEL_LIST_VALUE_CLASS {
 public:
  explicit TestListValue(const Handle<ListType>& type,
                         std::vector<int64_t> elements)
      : CEL_LIST_VALUE_CLASS(type), elements_(std::move(elements)) {
    ABSL_ASSERT(type->element()->Is<IntType>());
  }

  size_t size() const override { return elements_.size(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const override {
    if (index >= size()) {
      return absl::OutOfRangeError("");
    }
    return context.value_factory().CreateIntValue(elements_[index]);
  }

  std::string DebugString() const override {
    return absl::StrCat("[", absl::StrJoin(elements_, ", "), "]");
  }

  const std::vector<int64_t>& value() const { return elements_; }

 private:
  std::vector<int64_t> elements_;

  CEL_DECLARE_LIST_VALUE(TestListValue);
};

CEL_IMPLEMENT_LIST_VALUE(TestListValue);

class TestMapKeysListValue final : public CEL_LIST_VALUE_CLASS {
 public:
  explicit TestMapKeysListValue(const Handle<ListType>& type,
                                std::vector<std::string> elements)
      : CEL_LIST_VALUE_CLASS(type), elements_(std::move(elements)) {}

  size_t size() const override { return elements_.size(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const override {
    if (index >= size()) {
      return absl::OutOfRangeError("");
    }
    return context.value_factory().CreateStringValue(elements_[index]);
  }

  std::string DebugString() const override {
    return absl::StrCat("[", absl::StrJoin(elements_, ", "), "]");
  }

  const std::vector<std::string>& value() const { return elements_; }

 private:
  std::vector<std::string> elements_;

  CEL_DECLARE_LIST_VALUE(TestMapKeysListValue);
};

CEL_IMPLEMENT_LIST_VALUE(TestMapKeysListValue);

class TestMapValue final : public CEL_MAP_VALUE_CLASS {
 public:
  explicit TestMapValue(const Handle<MapType>& type,
                        std::map<std::string, int64_t> entries)
      : CEL_MAP_VALUE_CLASS(type), entries_(std::move(entries)) {
    ABSL_ASSERT(type->key()->Is<StringType>());
    ABSL_ASSERT(type->value()->Is<IntType>());
  }

  size_t size() const override { return entries_.size(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      const GetContext& context, const Handle<Value>& key) const override {
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError("");
    }
    auto entry = entries_.find(key.As<StringValue>()->ToString());
    if (entry == entries_.end()) {
      return absl::nullopt;
    }
    return context.value_factory().CreateIntValue(entry->second);
  }

  absl::StatusOr<bool> Has(const HasContext& context,
                           const Handle<Value>& key) const override {
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError("");
    }
    auto entry = entries_.find(key.As<StringValue>()->ToString());
    if (entry == entries_.end()) {
      return false;
    }
    return true;
  }

  std::string DebugString() const override {
    std::vector<std::string> parts;
    for (const auto& entry : entries_) {
      parts.push_back(absl::StrCat(internal::FormatStringLiteral(entry.first),
                                   ": ", entry.second));
    }
    return absl::StrCat("{", absl::StrJoin(parts, ", "), "}");
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      const ListKeysContext& context) const override {
    CEL_ASSIGN_OR_RETURN(
        auto list_type,
        context.value_factory().type_factory().CreateListType(
            context.value_factory().type_factory().GetStringType()));
    std::vector<std::string> keys;
    keys.reserve(entries_.size());
    for (const auto& entry : entries_) {
      keys.push_back(entry.first);
    }
    return context.value_factory().CreateListValue<TestMapKeysListValue>(
        std::move(list_type), std::move(keys));
  }

  const std::map<std::string, int64_t>& value() const { return entries_; }

 private:
  std::map<std::string, int64_t> entries_;

  CEL_DECLARE_MAP_VALUE(TestMapValue);
};

CEL_IMPLEMENT_MAP_VALUE(TestMapValue);

template <typename T>
T Must(absl::StatusOr<T> status_or_handle) {
  return std::move(status_or_handle).value();
}

template <class T>
constexpr void IS_INITIALIZED(T&) {}

template <typename... Types>
class BaseValueTest
    : public testing::TestWithParam<
          std::tuple<base_internal::MemoryManagerTestMode, Types...>> {
  using Base = testing::TestWithParam<
      std::tuple<base_internal::MemoryManagerTestMode, Types...>>;

 protected:
  void SetUp() override {
    if (std::get<0>(Base::GetParam()) ==
        base_internal::MemoryManagerTestMode::kArena) {
      memory_manager_ = ArenaMemoryManager::Default();
    }
  }

  void TearDown() override {
    if (std::get<0>(Base::GetParam()) ==
        base_internal::MemoryManagerTestMode::kArena) {
      memory_manager_.reset();
    }
  }

  MemoryManager& memory_manager() const {
    switch (std::get<0>(Base::GetParam())) {
      case base_internal::MemoryManagerTestMode::kGlobal:
        return MemoryManager::Global();
      case base_internal::MemoryManagerTestMode::kArena:
        return *memory_manager_;
    }
  }

  const auto& test_case() const { return std::get<1>(Base::GetParam()); }

 private:
  std::unique_ptr<ArenaMemoryManager> memory_manager_;
};

using ValueTest = BaseValueTest<>;

TEST(Value, HandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Handle<Value>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Handle<Value>>);
  EXPECT_TRUE(std::is_move_constructible_v<Handle<Value>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Handle<Value>>);
  EXPECT_TRUE(std::is_move_assignable_v<Handle<Value>>);
  EXPECT_TRUE(std::is_swappable_v<Handle<Value>>);
  EXPECT_TRUE(std::is_default_constructible_v<Handle<Value>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Handle<Value>>);
  EXPECT_TRUE(std::is_move_constructible_v<Handle<Value>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Handle<Value>>);
  EXPECT_TRUE(std::is_move_assignable_v<Handle<Value>>);
  EXPECT_TRUE(std::is_swappable_v<Handle<Value>>);
}

TEST_P(ValueTest, DefaultConstructor) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> value;
  EXPECT_FALSE(value);
}

struct ConstructionAssignmentTestCase final {
  std::string name;
  std::function<Handle<Value>(TypeFactory&, ValueFactory&)> default_value;
};

using ConstructionAssignmentTest =
    BaseValueTest<ConstructionAssignmentTestCase>;

TEST_P(ConstructionAssignmentTest, CopyConstructor) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> from(test_case().default_value(type_factory, value_factory));
  Handle<Value> to(from);
  IS_INITIALIZED(to);
  EXPECT_EQ(to, test_case().default_value(type_factory, value_factory));
}

TEST_P(ConstructionAssignmentTest, MoveConstructor) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> from(test_case().default_value(type_factory, value_factory));
  Handle<Value> to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_FALSE(from);
  EXPECT_EQ(to, test_case().default_value(type_factory, value_factory));
}

TEST_P(ConstructionAssignmentTest, CopyAssignment) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> from(test_case().default_value(type_factory, value_factory));
  Handle<Value> to;
  to = from;
  EXPECT_EQ(to, from);
}

TEST_P(ConstructionAssignmentTest, MoveAssignment) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> from(test_case().default_value(type_factory, value_factory));
  Handle<Value> to;
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_FALSE(from);
  EXPECT_EQ(to, test_case().default_value(type_factory, value_factory));
}

INSTANTIATE_TEST_SUITE_P(
    ConstructionAssignmentTest, ConstructionAssignmentTest,
    testing::Combine(
        base_internal::MemoryManagerTestModeAll(),
        testing::ValuesIn<ConstructionAssignmentTestCase>({
            {"Null",
             [](TypeFactory& type_factory, ValueFactory& value_factory)
                 -> Handle<Value> { return value_factory.GetNullValue(); }},
            {"Bool",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return value_factory.CreateBoolValue(false);
             }},
            {"Int",
             [](TypeFactory& type_factory, ValueFactory& value_factory)
                 -> Handle<Value> { return value_factory.CreateIntValue(0); }},
            {"Uint",
             [](TypeFactory& type_factory, ValueFactory& value_factory)
                 -> Handle<Value> { return value_factory.CreateUintValue(0); }},
            {"Double",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return value_factory.CreateDoubleValue(0.0);
             }},
            {"Duration",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(
                   value_factory.CreateDurationValue(absl::ZeroDuration()));
             }},
            {"Timestamp",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(
                   value_factory.CreateTimestampValue(absl::UnixEpoch()));
             }},
            {"Error",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return value_factory.CreateErrorValue(absl::CancelledError());
             }},
            {"Bytes",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(value_factory.CreateBytesValue(""));
             }},
            {"String",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(value_factory.CreateStringValue(""));
             }},
            {"Enum",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(value_factory.CreateEnumValue(
                   Must(type_factory.CreateEnumType<TestEnumType>()), 1));
             }},
            /*{"Struct",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(value_factory.CreateStructValue<TestStructValue>(
                   Must(type_factory.CreateStructType<TestStructType>())));
             }},
            {"List",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(value_factory.CreateListValue<TestListValue>(
                   Must(type_factory.CreateListType(type_factory.GetIntType())),
                   std::vector<int64_t>{}));
             }},
            {"Map",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return Must(value_factory.CreateMapValue<TestMapValue>(
                   Must(type_factory.CreateMapType(type_factory.GetStringType(),
                                                   type_factory.GetIntType())),
                   std::map<std::string, int64_t>{}));
             }},*/
            {"Type",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return value_factory.CreateTypeValue(type_factory.GetNullType());
             }},
            {"Unknown",
             [](TypeFactory& type_factory,
                ValueFactory& value_factory) -> Handle<Value> {
               return value_factory.CreateUnknownValue();
             }},
        })),
    [](const testing::TestParamInfo<
        std::tuple<base_internal::MemoryManagerTestMode,
                   ConstructionAssignmentTestCase>>& info) {
      return absl::StrCat(
          base_internal::MemoryManagerTestModeToString(std::get<0>(info.param)),
          "_", std::get<1>(info.param).name);
    });

TEST_P(ValueTest, Swap) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> lhs = value_factory.CreateIntValue(0);
  Handle<Value> rhs = value_factory.CreateUintValue(0);
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, value_factory.CreateUintValue(0));
  EXPECT_EQ(rhs, value_factory.CreateIntValue(0));
}

using ValueDebugStringTest = ValueTest;

TEST_P(ValueDebugStringTest, NullValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(value_factory.GetNullValue()->DebugString(), "null");
}

TEST_P(ValueDebugStringTest, BoolValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(value_factory.CreateBoolValue(false)->DebugString(), "false");
  EXPECT_EQ(value_factory.CreateBoolValue(true)->DebugString(), "true");
}

TEST_P(ValueDebugStringTest, IntValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(value_factory.CreateIntValue(-1)->DebugString(), "-1");
  EXPECT_EQ(value_factory.CreateIntValue(0)->DebugString(), "0");
  EXPECT_EQ(value_factory.CreateIntValue(1)->DebugString(), "1");
  EXPECT_EQ(value_factory.CreateIntValue(std::numeric_limits<int64_t>::min())
                ->DebugString(),
            "-9223372036854775808");
  EXPECT_EQ(value_factory.CreateIntValue(std::numeric_limits<int64_t>::max())
                ->DebugString(),
            "9223372036854775807");
}

TEST_P(ValueDebugStringTest, UintValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(value_factory.CreateUintValue(0)->DebugString(), "0u");
  EXPECT_EQ(value_factory.CreateUintValue(1)->DebugString(), "1u");
  EXPECT_EQ(value_factory.CreateUintValue(std::numeric_limits<uint64_t>::max())
                ->DebugString(),
            "18446744073709551615u");
}

TEST_P(ValueDebugStringTest, DoubleValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(value_factory.CreateDoubleValue(-1.0)->DebugString(), "-1.0");
  EXPECT_EQ(value_factory.CreateDoubleValue(0.0)->DebugString(), "0.0");
  EXPECT_EQ(value_factory.CreateDoubleValue(1.0)->DebugString(), "1.0");
  EXPECT_EQ(value_factory.CreateDoubleValue(-1.1)->DebugString(), "-1.1");
  EXPECT_EQ(value_factory.CreateDoubleValue(0.1)->DebugString(), "0.1");
  EXPECT_EQ(value_factory.CreateDoubleValue(1.1)->DebugString(), "1.1");
  EXPECT_EQ(value_factory.CreateDoubleValue(-9007199254740991.0)->DebugString(),
            "-9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(9007199254740991.0)->DebugString(),
            "9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(-9007199254740991.1)->DebugString(),
            "-9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(9007199254740991.1)->DebugString(),
            "9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(9007199254740991.1)->DebugString(),
            "9.0072e+15");

  EXPECT_EQ(
      value_factory.CreateDoubleValue(std::numeric_limits<double>::quiet_NaN())
          ->DebugString(),
      "nan");
  EXPECT_EQ(
      value_factory.CreateDoubleValue(std::numeric_limits<double>::infinity())
          ->DebugString(),
      "+infinity");
  EXPECT_EQ(
      value_factory.CreateDoubleValue(-std::numeric_limits<double>::infinity())
          ->DebugString(),
      "-infinity");
}

TEST_P(ValueDebugStringTest, DurationValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(DurationValue::Zero(value_factory)->DebugString(),
            internal::FormatDuration(absl::ZeroDuration()).value());
}

TEST_P(ValueDebugStringTest, TimestampValue) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(TimestampValue::UnixEpoch(value_factory)->DebugString(),
            internal::FormatTimestamp(absl::UnixEpoch()).value());
}

INSTANTIATE_TEST_SUITE_P(ValueDebugStringTest, ValueDebugStringTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeTupleName);

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

TEST_P(ValueTest, Error) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto error_value = value_factory.CreateErrorValue(absl::CancelledError());
  EXPECT_TRUE(error_value->Is<ErrorValue>());
  EXPECT_FALSE(error_value->Is<NullValue>());
  EXPECT_EQ(error_value, error_value);
  EXPECT_EQ(error_value,
            value_factory.CreateErrorValue(absl::CancelledError()));
  EXPECT_EQ(error_value->value(), absl::CancelledError());
}

TEST_P(ValueTest, Bool) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto false_value = BoolValue::False(value_factory);
  EXPECT_TRUE(false_value->Is<BoolValue>());
  EXPECT_FALSE(false_value->Is<NullValue>());
  EXPECT_EQ(false_value, false_value);
  EXPECT_EQ(false_value, value_factory.CreateBoolValue(false));
  EXPECT_EQ(false_value->kind(), ValueKind::kBool);
  EXPECT_EQ(false_value->type(), type_factory.GetBoolType());
  EXPECT_FALSE(false_value->value());

  auto true_value = BoolValue::True(value_factory);
  EXPECT_TRUE(true_value->Is<BoolValue>());
  EXPECT_FALSE(true_value->Is<NullValue>());
  EXPECT_EQ(true_value, true_value);
  EXPECT_EQ(true_value, value_factory.CreateBoolValue(true));
  EXPECT_EQ(true_value->kind(), ValueKind::kBool);
  EXPECT_EQ(true_value->type(), type_factory.GetBoolType());
  EXPECT_TRUE(true_value->value());

  EXPECT_NE(false_value, true_value);
  EXPECT_NE(true_value, false_value);
}

TEST_P(ValueTest, Int) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = value_factory.CreateIntValue(0);
  EXPECT_TRUE(zero_value->Is<IntValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateIntValue(0));
  EXPECT_EQ(zero_value->kind(), ValueKind::kInt);
  EXPECT_EQ(zero_value->type(), type_factory.GetIntType());
  EXPECT_EQ(zero_value->value(), 0);

  auto one_value = value_factory.CreateIntValue(1);
  EXPECT_TRUE(one_value->Is<IntValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, value_factory.CreateIntValue(1));
  EXPECT_EQ(one_value->kind(), ValueKind::kInt);
  EXPECT_EQ(one_value->type(), type_factory.GetIntType());
  EXPECT_EQ(one_value->value(), 1);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, Uint) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = value_factory.CreateUintValue(0);
  EXPECT_TRUE(zero_value->Is<UintValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateUintValue(0));
  EXPECT_EQ(zero_value->kind(), ValueKind::kUint);
  EXPECT_EQ(zero_value->type(), type_factory.GetUintType());
  EXPECT_EQ(zero_value->value(), 0);

  auto one_value = value_factory.CreateUintValue(1);
  EXPECT_TRUE(one_value->Is<UintValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, value_factory.CreateUintValue(1));
  EXPECT_EQ(one_value->kind(), ValueKind::kUint);
  EXPECT_EQ(one_value->type(), type_factory.GetUintType());
  EXPECT_EQ(one_value->value(), 1);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, Double) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = value_factory.CreateDoubleValue(0.0);
  EXPECT_TRUE(zero_value->Is<DoubleValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateDoubleValue(0.0));
  EXPECT_EQ(zero_value->kind(), ValueKind::kDouble);
  EXPECT_EQ(zero_value->type(), type_factory.GetDoubleType());
  EXPECT_EQ(zero_value->value(), 0.0);

  auto one_value = value_factory.CreateDoubleValue(1.0);
  EXPECT_TRUE(one_value->Is<DoubleValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, value_factory.CreateDoubleValue(1.0));
  EXPECT_EQ(one_value->kind(), ValueKind::kDouble);
  EXPECT_EQ(one_value->type(), type_factory.GetDoubleType());
  EXPECT_EQ(one_value->value(), 1.0);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, Duration) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value =
      Must(value_factory.CreateDurationValue(absl::ZeroDuration()));
  EXPECT_TRUE(zero_value->Is<DurationValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateDurationValue(absl::ZeroDuration())));
  EXPECT_EQ(zero_value->kind(), ValueKind::kDuration);
  EXPECT_EQ(zero_value->type(), type_factory.GetDurationType());
  EXPECT_EQ(zero_value->value(), absl::ZeroDuration());

  auto one_value = Must(value_factory.CreateDurationValue(
      absl::ZeroDuration() + absl::Nanoseconds(1)));
  EXPECT_TRUE(one_value->Is<DurationValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), ValueKind::kDuration);
  EXPECT_EQ(one_value->type(), type_factory.GetDurationType());
  EXPECT_EQ(one_value->value(), absl::ZeroDuration() + absl::Nanoseconds(1));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);

  EXPECT_THAT(value_factory.CreateDurationValue(absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ValueTest, Timestamp) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateTimestampValue(absl::UnixEpoch()));
  EXPECT_TRUE(zero_value->Is<TimestampValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateTimestampValue(absl::UnixEpoch())));
  EXPECT_EQ(zero_value->kind(), ValueKind::kTimestamp);
  EXPECT_EQ(zero_value->type(), type_factory.GetTimestampType());
  EXPECT_EQ(zero_value->value(), absl::UnixEpoch());

  auto one_value = Must(value_factory.CreateTimestampValue(
      absl::UnixEpoch() + absl::Nanoseconds(1)));
  EXPECT_TRUE(one_value->Is<TimestampValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), ValueKind::kTimestamp);
  EXPECT_EQ(one_value->type(), type_factory.GetTimestampType());
  EXPECT_EQ(one_value->value(), absl::UnixEpoch() + absl::Nanoseconds(1));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);

  EXPECT_THAT(value_factory.CreateTimestampValue(absl::InfiniteFuture()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ValueTest, BytesFromString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateBytesValue(std::string("0")));
  EXPECT_TRUE(zero_value->Is<BytesValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue(std::string("0"))));
  EXPECT_EQ(zero_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue(std::string("1")));
  EXPECT_TRUE(one_value->Is<BytesValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue(std::string("1"))));
  EXPECT_EQ(one_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, BytesFromStringView) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value =
      Must(value_factory.CreateBytesValue(absl::string_view("0")));
  EXPECT_TRUE(zero_value->Is<BytesValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateBytesValue(absl::string_view("0"))));
  EXPECT_EQ(zero_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue(absl::string_view("1")));
  EXPECT_TRUE(one_value->Is<BytesValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value,
            Must(value_factory.CreateBytesValue(absl::string_view("1"))));
  EXPECT_EQ(one_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, BytesFromCord) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateBytesValue(absl::Cord("0")));
  EXPECT_TRUE(zero_value->Is<BytesValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue(absl::Cord("0"))));
  EXPECT_EQ(zero_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToCord(), "0");

  auto one_value = Must(value_factory.CreateBytesValue(absl::Cord("1")));
  EXPECT_TRUE(one_value->Is<BytesValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue(absl::Cord("1"))));
  EXPECT_EQ(one_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToCord(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, BytesFromLiteral) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateBytesValue("0"));
  EXPECT_TRUE(zero_value->Is<BytesValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue("0")));
  EXPECT_EQ(zero_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue("1"));
  EXPECT_TRUE(one_value->Is<BytesValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue("1")));
  EXPECT_EQ(one_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, BytesFromExternal) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateBytesValue("0", []() {}));
  EXPECT_TRUE(zero_value->Is<BytesValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue("0", []() {})));
  EXPECT_EQ(zero_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue("1", []() {}));
  EXPECT_TRUE(one_value->Is<BytesValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue("1", []() {})));
  EXPECT_EQ(one_value->kind(), ValueKind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, StringFromString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateStringValue(std::string("0")));
  EXPECT_TRUE(zero_value->Is<StringValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateStringValue(std::string("0"))));
  EXPECT_EQ(zero_value->kind(), ValueKind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateStringValue(std::string("1")));
  EXPECT_TRUE(one_value->Is<StringValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue(std::string("1"))));
  EXPECT_EQ(one_value->kind(), ValueKind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, StringFromStringView) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value =
      Must(value_factory.CreateStringValue(absl::string_view("0")));
  EXPECT_TRUE(zero_value->Is<StringValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateStringValue(absl::string_view("0"))));
  EXPECT_EQ(zero_value->kind(), ValueKind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value =
      Must(value_factory.CreateStringValue(absl::string_view("1")));
  EXPECT_TRUE(one_value->Is<StringValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value,
            Must(value_factory.CreateStringValue(absl::string_view("1"))));
  EXPECT_EQ(one_value->kind(), ValueKind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, StringFromCord) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateStringValue(absl::Cord("0")));
  EXPECT_TRUE(zero_value->Is<StringValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateStringValue(absl::Cord("0"))));
  EXPECT_EQ(zero_value->kind(), ValueKind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToCord(), "0");

  auto one_value = Must(value_factory.CreateStringValue(absl::Cord("1")));
  EXPECT_TRUE(one_value->Is<StringValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue(absl::Cord("1"))));
  EXPECT_EQ(one_value->kind(), ValueKind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToCord(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, StringFromLiteral) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateStringValue("0"));
  EXPECT_TRUE(zero_value->Is<StringValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateStringValue("0")));
  EXPECT_EQ(zero_value->kind(), ValueKind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateStringValue("1"));
  EXPECT_TRUE(one_value->Is<StringValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue("1")));
  EXPECT_EQ(one_value->kind(), ValueKind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, StringFromExternal) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = Must(value_factory.CreateStringValue("0", []() {}));
  EXPECT_TRUE(zero_value->Is<StringValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateStringValue("0", []() {})));
  EXPECT_EQ(zero_value->kind(), ValueKind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateStringValue("1", []() {}));
  EXPECT_TRUE(one_value->Is<StringValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue("1", []() {})));
  EXPECT_EQ(one_value->kind(), ValueKind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST_P(ValueTest, Type) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto null_value = value_factory.CreateTypeValue(type_factory.GetNullType());
  EXPECT_TRUE(null_value->Is<TypeValue>());
  EXPECT_FALSE(null_value->Is<NullValue>());
  EXPECT_EQ(null_value, null_value);
  EXPECT_EQ(null_value,
            value_factory.CreateTypeValue(type_factory.GetNullType()));
  EXPECT_EQ(null_value->kind(), ValueKind::kType);
  EXPECT_EQ(null_value->type(), type_factory.GetTypeType());
  EXPECT_EQ(null_value->name(), "null_type");

  auto int_value = value_factory.CreateTypeValue(type_factory.GetIntType());
  EXPECT_TRUE(int_value->Is<TypeValue>());
  EXPECT_FALSE(int_value->Is<NullValue>());
  EXPECT_EQ(int_value, int_value);
  EXPECT_EQ(int_value,
            value_factory.CreateTypeValue(type_factory.GetIntType()));
  EXPECT_EQ(int_value->kind(), ValueKind::kType);
  EXPECT_EQ(int_value->type(), type_factory.GetTypeType());
  EXPECT_EQ(int_value->name(), "int");

  EXPECT_NE(null_value, int_value);
  EXPECT_NE(int_value, null_value);
}

TEST_P(ValueTest, Unknown) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto zero_value = value_factory.CreateUnknownValue();
  EXPECT_TRUE(zero_value->Is<UnknownValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateUnknownValue());
  EXPECT_EQ(zero_value->kind(), ValueKind::kUnknown);
  EXPECT_EQ(zero_value->type(), type_factory.GetUnknownType());
}

TEST_P(ValueTest, Optional) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto none_optional,
      OptionalValue::None(value_factory, type_factory.GetStringType()));
  EXPECT_TRUE(none_optional->Is<OpaqueValue>());
  EXPECT_TRUE(none_optional->Is<OptionalValue>());
  EXPECT_FALSE(none_optional->Is<NullValue>());
  EXPECT_EQ(none_optional, none_optional);
  EXPECT_EQ(none_optional->kind(), ValueKind::kOpaque);
  ASSERT_OK_AND_ASSIGN(auto optional_type, type_factory.CreateOptionalType(
                                               type_factory.GetStringType()));
  EXPECT_EQ(none_optional->type(), optional_type);
  EXPECT_FALSE(none_optional->has_value());
  EXPECT_EQ(none_optional->DebugString(), "optional()");

  ASSERT_OK_AND_ASSIGN(
      auto full_optional,
      OptionalValue::Of(value_factory, value_factory.GetStringValue()));
  EXPECT_TRUE(full_optional->Is<OpaqueValue>());
  EXPECT_TRUE(full_optional->Is<OptionalValue>());
  EXPECT_FALSE(full_optional->Is<NullValue>());
  EXPECT_EQ(full_optional, full_optional);
  EXPECT_EQ(full_optional->kind(), ValueKind::kOpaque);
  EXPECT_EQ(full_optional->type(), optional_type);
  EXPECT_TRUE(full_optional->has_value());
  EXPECT_EQ(full_optional->value(), value_factory.GetStringValue());
  EXPECT_EQ(full_optional->DebugString(), "optional(\"\")");

  EXPECT_NE(none_optional, full_optional);
  EXPECT_NE(full_optional, none_optional);
}

Handle<BytesValue> MakeStringBytes(ValueFactory& value_factory,
                                   absl::string_view value) {
  return Must(value_factory.CreateBytesValue(value));
}

Handle<BytesValue> MakeCordBytes(ValueFactory& value_factory,
                                 absl::string_view value) {
  return Must(value_factory.CreateBytesValue(absl::Cord(value)));
}

Handle<BytesValue> MakeExternalBytes(ValueFactory& value_factory,
                                     absl::string_view value) {
  return Must(value_factory.CreateBytesValue(value, []() {}));
}

struct BytesConcatTestCase final {
  std::string lhs;
  std::string rhs;
};

using BytesConcatTest = BaseValueTest<BytesConcatTestCase>;

TEST_P(BytesConcatTest, Concat) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              *MakeStringBytes(value_factory, test_case().lhs),
                              *MakeStringBytes(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              *MakeStringBytes(value_factory, test_case().lhs),
                              *MakeCordBytes(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(
               value_factory, *MakeStringBytes(value_factory, test_case().lhs),
               *MakeExternalBytes(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              *MakeCordBytes(value_factory, test_case().lhs),
                              *MakeStringBytes(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              *MakeCordBytes(value_factory, test_case().lhs),
                              *MakeCordBytes(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(
               value_factory, *MakeCordBytes(value_factory, test_case().lhs),
               *MakeExternalBytes(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(Must(BytesValue::Concat(
                       value_factory,
                       *MakeExternalBytes(value_factory, test_case().lhs),
                       *MakeStringBytes(value_factory, test_case().rhs)))
                  ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(Must(BytesValue::Concat(
                       value_factory,
                       *MakeExternalBytes(value_factory, test_case().lhs),
                       *MakeCordBytes(value_factory, test_case().rhs)))
                  ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(Must(BytesValue::Concat(
                       value_factory,
                       *MakeExternalBytes(value_factory, test_case().lhs),
                       *MakeExternalBytes(value_factory, test_case().rhs)))
                  ->Equals(test_case().lhs + test_case().rhs));
}

INSTANTIATE_TEST_SUITE_P(
    BytesConcatTest, BytesConcatTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesConcatTestCase>({
                         {"", ""},
                         {"", std::string("\0", 1)},
                         {std::string("\0", 1), ""},
                         {std::string("\0", 1), std::string("\0", 1)},
                         {"", "foo"},
                         {"foo", ""},
                         {"foo", "foo"},
                         {"bar", "foo"},
                         {"foo", "bar"},
                         {"bar", "bar"},
                     })));

struct BytesSizeTestCase final {
  std::string data;
  size_t size;
};

using BytesSizeTest = BaseValueTest<BytesSizeTestCase>;

TEST_P(BytesSizeTest, Size) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().data)->size(),
            test_case().size);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().data)->size(),
            test_case().size);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().data)->size(),
            test_case().size);
}

INSTANTIATE_TEST_SUITE_P(
    BytesSizeTest, BytesSizeTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesSizeTestCase>({
                         {"", 0},
                         {"1", 1},
                         {"foo", 3},
                         {"\xef\xbf\xbd", 3},
                     })));

struct BytesEmptyTestCase final {
  std::string data;
  bool empty;
};

using BytesEmptyTest = BaseValueTest<BytesEmptyTestCase>;

TEST_P(BytesEmptyTest, Empty) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().data)->empty(),
            test_case().empty);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().data)->empty(),
            test_case().empty);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().data)->empty(),
            test_case().empty);
}

INSTANTIATE_TEST_SUITE_P(
    BytesEmptyTest, BytesEmptyTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesEmptyTestCase>({
                         {"", true},
                         {std::string("\0", 1), false},
                         {"1", false},
                     })));

struct BytesEqualsTestCase final {
  std::string lhs;
  std::string rhs;
  bool equals;
};

using BytesEqualsTest = BaseValueTest<BytesEqualsTestCase>;

TEST_P(BytesEqualsTest, Equals) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().lhs)
                ->Equals(*MakeStringBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().lhs)
                ->Equals(*MakeCordBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().lhs)
                ->Equals(*MakeExternalBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().lhs)
                ->Equals(*MakeStringBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().lhs)
                ->Equals(*MakeCordBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().lhs)
                ->Equals(*MakeExternalBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().lhs)
                ->Equals(*MakeStringBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().lhs)
                ->Equals(*MakeCordBytes(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().lhs)
                ->Equals(*MakeExternalBytes(value_factory, test_case().rhs)),
            test_case().equals);
}

INSTANTIATE_TEST_SUITE_P(
    BytesEqualsTest, BytesEqualsTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesEqualsTestCase>({
                         {"", "", true},
                         {"", std::string("\0", 1), false},
                         {std::string("\0", 1), "", false},
                         {std::string("\0", 1), std::string("\0", 1), true},
                         {"", "foo", false},
                         {"foo", "", false},
                         {"foo", "foo", true},
                         {"bar", "foo", false},
                         {"foo", "bar", false},
                         {"bar", "bar", true},
                     })));

struct BytesCompareTestCase final {
  std::string lhs;
  std::string rhs;
  int compare;
};

using BytesCompareTest = BaseValueTest<BytesCompareTestCase>;

int NormalizeCompareResult(int compare) { return std::clamp(compare, -1, 1); }

TEST_P(BytesCompareTest, Equals) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeStringBytes(value_factory, test_case().lhs)
              ->Compare(*MakeStringBytes(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(value_factory, test_case().lhs)
                    ->Compare(*MakeCordBytes(value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeStringBytes(value_factory, test_case().lhs)
              ->Compare(*MakeExternalBytes(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(MakeCordBytes(value_factory, test_case().lhs)
                                       ->Compare(*MakeStringBytes(
                                           value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(value_factory, test_case().lhs)
                    ->Compare(*MakeCordBytes(value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(MakeCordBytes(value_factory, test_case().lhs)
                                       ->Compare(*MakeExternalBytes(
                                           value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeExternalBytes(value_factory, test_case().lhs)
              ->Compare(*MakeStringBytes(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalBytes(value_factory, test_case().lhs)
                    ->Compare(*MakeCordBytes(value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeExternalBytes(value_factory, test_case().lhs)
              ->Compare(*MakeExternalBytes(value_factory, test_case().rhs))),
      test_case().compare);
}

INSTANTIATE_TEST_SUITE_P(
    BytesCompareTest, BytesCompareTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesCompareTestCase>({
                         {"", "", 0},
                         {"", std::string("\0", 1), -1},
                         {std::string("\0", 1), "", 1},
                         {std::string("\0", 1), std::string("\0", 1), 0},
                         {"", "foo", -1},
                         {"foo", "", 1},
                         {"foo", "foo", 0},
                         {"bar", "foo", -1},
                         {"foo", "bar", 1},
                         {"bar", "bar", 0},
                     })));

struct BytesDebugStringTestCase final {
  std::string data;
};

using BytesDebugStringTest = BaseValueTest<BytesDebugStringTestCase>;

TEST_P(BytesDebugStringTest, ToCord) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().data)->DebugString(),
            internal::FormatBytesLiteral(test_case().data));
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().data)->DebugString(),
            internal::FormatBytesLiteral(test_case().data));
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().data)->DebugString(),
            internal::FormatBytesLiteral(test_case().data));
}

INSTANTIATE_TEST_SUITE_P(
    BytesDebugStringTest, BytesDebugStringTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesDebugStringTestCase>({
                         {""},
                         {"1"},
                         {"foo"},
                         {"\xef\xbf\xbd"},
                     })));

struct BytesToStringTestCase final {
  std::string data;
};

using BytesToStringTest = BaseValueTest<BytesToStringTestCase>;

TEST_P(BytesToStringTest, ToString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().data)->ToString(),
            test_case().data);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().data)->ToString(),
            test_case().data);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().data)->ToString(),
            test_case().data);
}

INSTANTIATE_TEST_SUITE_P(
    BytesToStringTest, BytesToStringTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesToStringTestCase>({
                         {""},
                         {"1"},
                         {"foo"},
                         {"\xef\xbf\xbd"},
                     })));

struct BytesToCordTestCase final {
  std::string data;
};

using BytesToCordTest = BaseValueTest<BytesToCordTestCase>;

TEST_P(BytesToCordTest, ToCord) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case().data)->ToCord(),
            test_case().data);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case().data)->ToCord(),
            test_case().data);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case().data)->ToCord(),
            test_case().data);
}

INSTANTIATE_TEST_SUITE_P(
    BytesToCordTest, BytesToCordTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<BytesToCordTestCase>({
                         {""},
                         {"1"},
                         {"foo"},
                         {"\xef\xbf\xbd"},
                     })));

Handle<StringValue> MakeStringString(ValueFactory& value_factory,
                                     absl::string_view value) {
  return Must(value_factory.CreateStringValue(value));
}

Handle<StringValue> MakeCordString(ValueFactory& value_factory,
                                   absl::string_view value) {
  return Must(value_factory.CreateStringValue(absl::Cord(value)));
}

Handle<StringValue> MakeExternalString(ValueFactory& value_factory,
                                       absl::string_view value) {
  return Must(value_factory.CreateStringValue(value, []() {}));
}

struct StringConcatTestCase final {
  std::string lhs;
  std::string rhs;
};

using StringConcatTest = BaseValueTest<StringConcatTestCase>;

TEST_P(StringConcatTest, Concat) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, *MakeStringString(value_factory, test_case().lhs),
               *MakeStringString(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, *MakeStringString(value_factory, test_case().lhs),
               *MakeCordString(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, *MakeStringString(value_factory, test_case().lhs),
               *MakeExternalString(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, *MakeCordString(value_factory, test_case().lhs),
               *MakeStringString(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               *MakeCordString(value_factory, test_case().lhs),
                               *MakeCordString(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, *MakeCordString(value_factory, test_case().lhs),
               *MakeExternalString(value_factory, test_case().rhs)))
          ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(Must(StringValue::Concat(
                       value_factory,
                       *MakeExternalString(value_factory, test_case().lhs),
                       *MakeStringString(value_factory, test_case().rhs)))
                  ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(Must(StringValue::Concat(
                       value_factory,
                       *MakeExternalString(value_factory, test_case().lhs),
                       *MakeCordString(value_factory, test_case().rhs)))
                  ->Equals(test_case().lhs + test_case().rhs));
  EXPECT_TRUE(Must(StringValue::Concat(
                       value_factory,
                       *MakeExternalString(value_factory, test_case().lhs),
                       *MakeExternalString(value_factory, test_case().rhs)))
                  ->Equals(test_case().lhs + test_case().rhs));
}

INSTANTIATE_TEST_SUITE_P(
    StringConcatTest, StringConcatTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringConcatTestCase>({
                         {"", ""},
                         {"", std::string("\0", 1)},
                         {std::string("\0", 1), ""},
                         {std::string("\0", 1), std::string("\0", 1)},
                         {"", "foo"},
                         {"foo", ""},
                         {"foo", "foo"},
                         {"bar", "foo"},
                         {"foo", "bar"},
                         {"bar", "bar"},
                     })));

struct StringMatchesTestCase final {
  std::string pattern;
  std::string subject;
  bool matches;
};

using StringMatchesTest = BaseValueTest<StringMatchesTestCase>;

TEST_P(StringMatchesTest, Matches) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  RE2 re(test_case().pattern);
  EXPECT_EQ(
      Must(value_factory.CreateStringValue(test_case().subject))->Matches(re),
      test_case().matches);
  EXPECT_EQ(
      Must(value_factory.CreateStringValue(absl::Cord(test_case().subject)))
          ->Matches(re),
      test_case().matches);
}

INSTANTIATE_TEST_SUITE_P(
    StringMatchesTest, StringMatchesTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringMatchesTestCase>({
                         {"", "", true},
                         {"foo", "foo", true},
                         {"foo", "bar", false},
                     })));

struct StringSizeTestCase final {
  std::string data;
  size_t size;
};

using StringSizeTest = BaseValueTest<StringSizeTestCase>;

TEST_P(StringSizeTest, Size) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringString(value_factory, test_case().data)->size(),
            test_case().size);
  EXPECT_EQ(MakeCordString(value_factory, test_case().data)->size(),
            test_case().size);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().data)->size(),
            test_case().size);
}

INSTANTIATE_TEST_SUITE_P(
    StringSizeTest, StringSizeTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringSizeTestCase>({
                         {"", 0},
                         {"1", 1},
                         {"foo", 3},
                         {"\xef\xbf\xbd", 1},
                     })));

struct StringEmptyTestCase final {
  std::string data;
  bool empty;
};

using StringEmptyTest = BaseValueTest<StringEmptyTestCase>;

TEST_P(StringEmptyTest, Empty) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringString(value_factory, test_case().data)->empty(),
            test_case().empty);
  EXPECT_EQ(MakeCordString(value_factory, test_case().data)->empty(),
            test_case().empty);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().data)->empty(),
            test_case().empty);
}

INSTANTIATE_TEST_SUITE_P(
    StringEmptyTest, StringEmptyTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringEmptyTestCase>({
                         {"", true},
                         {std::string("\0", 1), false},
                         {"1", false},
                     })));

struct StringEqualsTestCase final {
  std::string lhs;
  std::string rhs;
  bool equals;
};

using StringEqualsTest = BaseValueTest<StringEqualsTestCase>;

TEST_P(StringEqualsTest, Equals) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringString(value_factory, test_case().lhs)
                ->Equals(*MakeStringString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeStringString(value_factory, test_case().lhs)
                ->Equals(*MakeCordString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeStringString(value_factory, test_case().lhs)
                ->Equals(*MakeExternalString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeCordString(value_factory, test_case().lhs)
                ->Equals(*MakeStringString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeCordString(value_factory, test_case().lhs)
                ->Equals(*MakeCordString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeCordString(value_factory, test_case().lhs)
                ->Equals(*MakeExternalString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().lhs)
                ->Equals(*MakeStringString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().lhs)
                ->Equals(*MakeCordString(value_factory, test_case().rhs)),
            test_case().equals);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().lhs)
                ->Equals(*MakeExternalString(value_factory, test_case().rhs)),
            test_case().equals);
}

INSTANTIATE_TEST_SUITE_P(
    StringEqualsTest, StringEqualsTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringEqualsTestCase>({
                         {"", "", true},
                         {"", std::string("\0", 1), false},
                         {std::string("\0", 1), "", false},
                         {std::string("\0", 1), std::string("\0", 1), true},
                         {"", "foo", false},
                         {"foo", "", false},
                         {"foo", "foo", true},
                         {"bar", "foo", false},
                         {"foo", "bar", false},
                         {"bar", "bar", true},
                     })));

struct StringCompareTestCase final {
  std::string lhs;
  std::string rhs;
  int compare;
};

using StringCompareTest = BaseValueTest<StringCompareTestCase>;

TEST_P(StringCompareTest, Equals) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeStringString(value_factory, test_case().lhs)
              ->Compare(*MakeStringString(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringString(value_factory, test_case().lhs)
                    ->Compare(*MakeCordString(value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeStringString(value_factory, test_case().lhs)
              ->Compare(*MakeExternalString(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeCordString(value_factory, test_case().lhs)
              ->Compare(*MakeStringString(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordString(value_factory, test_case().lhs)
                    ->Compare(*MakeCordString(value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeCordString(value_factory, test_case().lhs)
              ->Compare(*MakeExternalString(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeExternalString(value_factory, test_case().lhs)
              ->Compare(*MakeStringString(value_factory, test_case().rhs))),
      test_case().compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalString(value_factory, test_case().lhs)
                    ->Compare(*MakeCordString(value_factory, test_case().rhs))),
            test_case().compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeExternalString(value_factory, test_case().lhs)
              ->Compare(*MakeExternalString(value_factory, test_case().rhs))),
      test_case().compare);
}

INSTANTIATE_TEST_SUITE_P(
    StringCompareTest, StringCompareTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringCompareTestCase>({
                         {"", "", 0},
                         {"", std::string("\0", 1), -1},
                         {std::string("\0", 1), "", 1},
                         {std::string("\0", 1), std::string("\0", 1), 0},
                         {"", "foo", -1},
                         {"foo", "", 1},
                         {"foo", "foo", 0},
                         {"bar", "foo", -1},
                         {"foo", "bar", 1},
                         {"bar", "bar", 0},
                     })));

struct StringDebugStringTestCase final {
  std::string data;
};

using StringDebugStringTest = BaseValueTest<StringDebugStringTestCase>;

TEST_P(StringDebugStringTest, ToCord) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringString(value_factory, test_case().data)->DebugString(),
            internal::FormatStringLiteral(test_case().data));
  EXPECT_EQ(MakeCordString(value_factory, test_case().data)->DebugString(),
            internal::FormatStringLiteral(test_case().data));
  EXPECT_EQ(MakeExternalString(value_factory, test_case().data)->DebugString(),
            internal::FormatStringLiteral(test_case().data));
}

INSTANTIATE_TEST_SUITE_P(
    StringDebugStringTest, StringDebugStringTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringDebugStringTestCase>({
                         {""},
                         {"1"},
                         {"foo"},
                         {"\xef\xbf\xbd"},
                     })));

struct StringToStringTestCase final {
  std::string data;
};

using StringToStringTest = BaseValueTest<StringToStringTestCase>;

TEST_P(StringToStringTest, ToString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringString(value_factory, test_case().data)->ToString(),
            test_case().data);
  EXPECT_EQ(MakeCordString(value_factory, test_case().data)->ToString(),
            test_case().data);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().data)->ToString(),
            test_case().data);
}

INSTANTIATE_TEST_SUITE_P(
    StringToStringTest, StringToStringTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringToStringTestCase>({
                         {""},
                         {"1"},
                         {"foo"},
                         {"\xef\xbf\xbd"},
                     })));

struct StringToCordTestCase final {
  std::string data;
};

using StringToCordTest = BaseValueTest<StringToCordTestCase>;

TEST_P(StringToCordTest, ToCord) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_EQ(MakeStringString(value_factory, test_case().data)->ToCord(),
            test_case().data);
  EXPECT_EQ(MakeCordString(value_factory, test_case().data)->ToCord(),
            test_case().data);
  EXPECT_EQ(MakeExternalString(value_factory, test_case().data)->ToCord(),
            test_case().data);
}

INSTANTIATE_TEST_SUITE_P(
    StringToCordTest, StringToCordTest,
    testing::Combine(base_internal::MemoryManagerTestModeAll(),
                     testing::ValuesIn<StringToCordTestCase>({
                         {""},
                         {"1"},
                         {"foo"},
                         {"\xef\xbf\xbd"},
                     })));

TEST_P(ValueTest, Enum) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  ASSERT_OK_AND_ASSIGN(auto one_value,
                       value_factory.CreateEnumValue(enum_type, "VALUE1"));
  EXPECT_TRUE(one_value->Is<EnumValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value,
            Must(value_factory.CreateEnumValue(enum_type, "VALUE1")));
  EXPECT_EQ(one_value->kind(), ValueKind::kEnum);
  EXPECT_EQ(one_value->type(), enum_type);
  EXPECT_EQ(one_value->name(), "VALUE1");
  EXPECT_EQ(one_value->number(), 1);

  ASSERT_OK_AND_ASSIGN(auto two_value,
                       value_factory.CreateEnumValue(enum_type, "VALUE2"));
  EXPECT_TRUE(two_value->Is<EnumValue>());
  EXPECT_FALSE(two_value->Is<NullValue>());
  EXPECT_EQ(two_value, two_value);
  EXPECT_EQ(two_value->kind(), ValueKind::kEnum);
  EXPECT_EQ(two_value->type(), enum_type);
  EXPECT_EQ(two_value->name(), "VALUE2");
  EXPECT_EQ(two_value->number(), 2);

  EXPECT_NE(one_value, two_value);
  EXPECT_NE(two_value, one_value);
}

using EnumValueTest = ValueTest;

TEST_P(EnumValueTest, NewInstance) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  ASSERT_OK_AND_ASSIGN(auto one_value,
                       value_factory.CreateEnumValue(enum_type, "VALUE1"));
  ASSERT_OK_AND_ASSIGN(auto two_value,
                       value_factory.CreateEnumValue(enum_type, "VALUE2"));
  ASSERT_OK_AND_ASSIGN(auto one_value_by_number,
                       value_factory.CreateEnumValue(enum_type, 1));
  ASSERT_OK_AND_ASSIGN(auto two_value_by_number,
                       value_factory.CreateEnumValue(enum_type, 2));
  EXPECT_EQ(one_value, one_value_by_number);
  EXPECT_EQ(two_value, two_value_by_number);

  EXPECT_THAT(value_factory.CreateEnumValue(enum_type, "VALUE3"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(value_factory.CreateEnumValue(enum_type, 3),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(EnumValueTest, UnknownConstantDebugString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  EXPECT_EQ(EnumValue::DebugString(*enum_type, 3), "test_enum.TestEnum(3)");
}

INSTANTIATE_TEST_SUITE_P(EnumValueTest, EnumValueTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeTupleName);

TEST_P(ValueTest, Struct) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto struct_type,
                       type_factory.CreateStructType<TestStructType>());
  ASSERT_OK_AND_ASSIGN(
      auto zero_value,
      value_factory.CreateStructValue<TestStructValue>(struct_type));
  EXPECT_TRUE(zero_value->Is<StructValue>());
  EXPECT_TRUE(zero_value->Is<TestStructValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value->kind(), ValueKind::kStruct);
  EXPECT_EQ(zero_value->type(), struct_type);
  EXPECT_EQ(zero_value.As<TestStructValue>()->value(), TestStruct{});

  ASSERT_OK_AND_ASSIGN(auto one_value,
                       value_factory.CreateStructValue<TestStructValue>(
                           struct_type, TestStruct{true, 1, 1, 1.0}));
  EXPECT_TRUE(one_value->Is<StructValue>());
  EXPECT_TRUE(one_value->Is<TestStructValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), ValueKind::kStruct);
  EXPECT_EQ(one_value->type(), struct_type);
  EXPECT_EQ(one_value.As<TestStructValue>()->value(),
            (TestStruct{true, 1, 1, 1.0}));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

using StructValueTest = ValueTest;

TEST_P(StructValueTest, GetField) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto struct_type,
                       type_factory.CreateStructType<TestStructType>());
  ASSERT_OK_AND_ASSIGN(
      auto struct_value,
      value_factory.CreateStructValue<TestStructValue>(struct_type));
  StructValue::GetFieldContext context(value_factory);
  EXPECT_THAT(struct_value->GetFieldByName(context, "bool_field"),
              IsOkAndHolds(Eq(value_factory.CreateBoolValue(false))));
  EXPECT_THAT(struct_value->GetFieldByNumber(context, 0),
              IsOkAndHolds(Eq(value_factory.CreateBoolValue(false))));
  EXPECT_THAT(struct_value->GetFieldByName(context, "int_field"),
              IsOkAndHolds(Eq(value_factory.CreateIntValue(0))));
  EXPECT_THAT(struct_value->GetFieldByNumber(context, 1),
              IsOkAndHolds(Eq(value_factory.CreateIntValue(0))));
  EXPECT_THAT(struct_value->GetFieldByName(context, "uint_field"),
              IsOkAndHolds(Eq(value_factory.CreateUintValue(0))));
  EXPECT_THAT(struct_value->GetFieldByNumber(context, 2),
              IsOkAndHolds(Eq(value_factory.CreateUintValue(0))));
  EXPECT_THAT(struct_value->GetFieldByName(context, "double_field"),
              IsOkAndHolds(Eq(value_factory.CreateDoubleValue(0.0))));
  EXPECT_THAT(struct_value->GetFieldByNumber(context, 3),
              IsOkAndHolds(Eq(value_factory.CreateDoubleValue(0.0))));
  EXPECT_THAT(struct_value->GetFieldByName(context, "missing_field"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(struct_value->HasFieldByNumber(
                  StructValue::HasFieldContext((type_manager)), 4),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(StructValueTest, HasField) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto struct_type,
                       type_factory.CreateStructType<TestStructType>());
  ASSERT_OK_AND_ASSIGN(
      auto struct_value,
      value_factory.CreateStructValue<TestStructValue>(struct_type));
  StructValue::HasFieldContext context(type_manager);
  EXPECT_THAT(struct_value->HasFieldByName(context, "bool_field"),
              IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByNumber(context, 0), IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByName(context, "int_field"),
              IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByNumber(context, 1), IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByName(context, "uint_field"),
              IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByNumber(context, 2), IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByName(context, "double_field"),
              IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByNumber(context, 3), IsOkAndHolds(true));
  EXPECT_THAT(struct_value->HasFieldByName(context, "missing_field"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(struct_value->HasFieldByNumber(context, 4),
              StatusIs(absl::StatusCode::kNotFound));
}

INSTANTIATE_TEST_SUITE_P(StructValueTest, StructValueTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeTupleName);

TEST_P(ValueTest, List) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto zero_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{}));
  EXPECT_TRUE(zero_value->Is<ListValue>());
  EXPECT_TRUE(zero_value->Is<TestListValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value->kind(), ValueKind::kList);
  EXPECT_EQ(zero_value->type(), list_type);
  EXPECT_EQ(zero_value.As<TestListValue>()->value(), std::vector<int64_t>{});

  ASSERT_OK_AND_ASSIGN(auto one_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{1}));
  EXPECT_TRUE(one_value->Is<ListValue>());
  EXPECT_TRUE(one_value->Is<TestListValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), ValueKind::kList);
  EXPECT_EQ(one_value->type(), list_type);
  EXPECT_EQ(one_value.As<TestListValue>()->value(), std::vector<int64_t>{1});

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

using ListValueTest = ValueTest;

TEST_P(ListValueTest, DebugString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto list_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{}));
  EXPECT_EQ(list_value->DebugString(), "[]");
  ASSERT_OK_AND_ASSIGN(list_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{0, 1, 2, 3, 4, 5}));
  EXPECT_EQ(list_value->DebugString(), "[0, 1, 2, 3, 4, 5]");
}

TEST_P(ListValueTest, Get) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto list_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{}));
  EXPECT_TRUE(list_value->empty());
  EXPECT_EQ(list_value->size(), 0);

  ASSERT_OK_AND_ASSIGN(list_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{0, 1, 2}));
  EXPECT_FALSE(list_value->empty());
  EXPECT_EQ(list_value->size(), 3);
  ListValue::GetContext context(value_factory);
  EXPECT_EQ(Must(list_value->Get(context, 0)), value_factory.CreateIntValue(0));
  EXPECT_EQ(Must(list_value->Get(context, 1)), value_factory.CreateIntValue(1));
  EXPECT_EQ(Must(list_value->Get(context, 2)), value_factory.CreateIntValue(2));
  EXPECT_THAT(list_value->Get(context, 3),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_P(ListValueTest, NewIteratorIndices) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto list_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{0, 1, 2}));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       list_value->NewIterator(memory_manager()));
  std::set<size_t> actual_indices;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto index, iterator->NextIndex(ListValue::GetContext(value_factory)));
    actual_indices.insert(index);
  }
  EXPECT_THAT(iterator->NextIndex(ListValue::GetContext(value_factory)),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<size_t> expected_indices = {0, 1, 2};
  EXPECT_EQ(actual_indices, expected_indices);
}

TEST_P(ListValueTest, NewIteratorValues) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto list_value,
                       value_factory.CreateListValue<TestListValue>(
                           list_type, std::vector<int64_t>{3, 4, 5}));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       list_value->NewIterator(memory_manager()));
  std::set<int64_t> actual_values;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto value, iterator->NextValue(ListValue::GetContext(value_factory)));
    actual_values.insert(value->As<IntValue>().value());
  }
  EXPECT_THAT(iterator->NextValue(ListValue::GetContext(value_factory)),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<int64_t> expected_values = {3, 4, 5};
  EXPECT_EQ(actual_values, expected_values);
}

INSTANTIATE_TEST_SUITE_P(ListValueTest, ListValueTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeTupleName);

TEST_P(ValueTest, Map) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto zero_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{}));
  EXPECT_TRUE(zero_value->Is<MapValue>());
  EXPECT_TRUE(zero_value->Is<TestMapValue>());
  EXPECT_FALSE(zero_value->Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value->kind(), ValueKind::kMap);
  EXPECT_EQ(zero_value->type(), map_type);
  EXPECT_EQ(zero_value.As<TestMapValue>()->value(),
            (std::map<std::string, int64_t>{}));

  ASSERT_OK_AND_ASSIGN(
      auto one_value,
      value_factory.CreateMapValue<TestMapValue>(
          map_type, std::map<std::string, int64_t>{{"foo", 1}}));
  EXPECT_TRUE(one_value->Is<MapValue>());
  EXPECT_TRUE(one_value->Is<TestMapValue>());
  EXPECT_FALSE(one_value->Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), ValueKind::kMap);
  EXPECT_EQ(one_value->type(), map_type);
  EXPECT_EQ(one_value.As<TestMapValue>()->value(),
            (std::map<std::string, int64_t>{{"foo", 1}}));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

using MapValueTest = ValueTest;

TEST_P(MapValueTest, DebugString) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{}));
  EXPECT_EQ(map_value->DebugString(), "{}");
  ASSERT_OK_AND_ASSIGN(map_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{
                                         {"foo", 1}, {"bar", 2}, {"baz", 3}}));
  EXPECT_EQ(map_value->DebugString(), "{\"bar\": 2, \"baz\": 3, \"foo\": 1}");
}

TEST_P(MapValueTest, GetAndHas) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{}));
  EXPECT_TRUE(map_value->empty());
  EXPECT_EQ(map_value->size(), 0);

  ASSERT_OK_AND_ASSIGN(map_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{
                                         {"foo", 1}, {"bar", 2}, {"baz", 3}}));
  EXPECT_FALSE(map_value->empty());
  EXPECT_EQ(map_value->size(), 3);
  EXPECT_EQ(Must(map_value->Get(MapValue::GetContext(value_factory),
                                Must(value_factory.CreateStringValue("foo")))),
            value_factory.CreateIntValue(1));
  EXPECT_THAT(map_value->Has(MapValue::HasContext(),
                             Must(value_factory.CreateStringValue("foo"))),
              IsOkAndHolds(true));
  EXPECT_EQ(Must(map_value->Get(MapValue::GetContext(value_factory),
                                Must(value_factory.CreateStringValue("bar")))),
            value_factory.CreateIntValue(2));
  EXPECT_THAT(map_value->Has(MapValue::HasContext(),
                             Must(value_factory.CreateStringValue("bar"))),
              IsOkAndHolds(true));
  EXPECT_EQ(Must(map_value->Get(MapValue::GetContext(value_factory),
                                Must(value_factory.CreateStringValue("baz")))),
            value_factory.CreateIntValue(3));
  EXPECT_THAT(map_value->Has(MapValue::HasContext(),
                             Must(value_factory.CreateStringValue("baz"))),
              IsOkAndHolds(true));
  EXPECT_THAT(map_value->Get(MapValue::GetContext(value_factory),
                             value_factory.CreateIntValue(0)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(map_value->Get(MapValue::GetContext(value_factory),
                             Must(value_factory.CreateStringValue("missing"))),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(map_value->Has(MapValue::HasContext(),
                             Must(value_factory.CreateStringValue("missing"))),
              IsOkAndHolds(false));
}

TEST_P(MapValueTest, NewIteratorKeys) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{
                                         {"foo", 1}, {"bar", 2}, {"baz", 3}}));
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value->NewIterator(memory_manager()));
  std::set<std::string> actual_keys;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto key, iterator->NextKey(MapValue::GetContext(value_factory)));
    actual_keys.insert(key->As<StringValue>().ToString());
  }
  EXPECT_THAT(iterator->NextKey(MapValue::GetContext(value_factory)),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<std::string> expected_keys = {"foo", "bar", "baz"};
  EXPECT_EQ(actual_keys, expected_keys);
}

TEST_P(MapValueTest, NewIteratorValues) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       value_factory.CreateMapValue<TestMapValue>(
                           map_type, std::map<std::string, int64_t>{
                                         {"foo", 1}, {"bar", 2}, {"baz", 3}}));
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value->NewIterator(memory_manager()));
  std::set<int64_t> actual_values;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto value, iterator->NextValue(MapValue::GetContext(value_factory)));
    actual_values.insert(value->As<IntValue>().value());
  }
  EXPECT_THAT(iterator->NextValue(MapValue::GetContext(value_factory)),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<int64_t> expected_values = {1, 2, 3};
  EXPECT_EQ(actual_values, expected_values);
}

INSTANTIATE_TEST_SUITE_P(MapValueTest, MapValueTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeTupleName);

INSTANTIATE_TEST_SUITE_P(ValueTest, ValueTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeTupleName);

TEST(TypeValue, SkippableDestructor) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto type_value = value_factory.CreateTypeValue(type_factory.GetBoolType());
  EXPECT_TRUE(base_internal::Metadata::IsDestructorSkippable(*type_value));
}

Handle<NullValue> DefaultNullValue(ValueFactory& value_factory) {
  return value_factory.GetNullValue();
}

Handle<ErrorValue> DefaultErrorValue(ValueFactory& value_factory) {
  return value_factory.CreateErrorValue(absl::CancelledError());
}

Handle<BoolValue> DefaultBoolValue(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(false);
}

Handle<IntValue> DefaultIntValue(ValueFactory& value_factory) {
  return value_factory.CreateIntValue(0);
}

Handle<UintValue> DefaultUintValue(ValueFactory& value_factory) {
  return value_factory.CreateUintValue(0);
}

Handle<DoubleValue> DefaultDoubleValue(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(0.0);
}

Handle<DurationValue> DefaultDurationValue(ValueFactory& value_factory) {
  return Must(value_factory.CreateDurationValue(absl::ZeroDuration()));
}

Handle<TimestampValue> DefaultTimestampValue(ValueFactory& value_factory) {
  return Must(value_factory.CreateTimestampValue(absl::UnixEpoch()));
}

Handle<TypeValue> DefaultTypeValue(ValueFactory& value_factory) {
  return value_factory.CreateTypeValue(
      value_factory.type_factory().GetNullType());
}

#define BM_SIMPLE_VALUES_LIST(XX) \
  XX(NullValue)                   \
  XX(ErrorValue)                  \
  XX(BoolValue)                   \
  XX(IntValue)                    \
  XX(UintValue)                   \
  XX(DoubleValue)                 \
  XX(DurationValue)               \
  XX(TimestampValue)              \
  XX(TypeValue)

template <typename T, Handle<T> (*F)(ValueFactory&)>
void BM_SimpleCopyConstruct(benchmark::State& state) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  Handle<Value> value = (*F)(value_factory);
  for (auto s : state) {
    Handle<Value> other(value);
    benchmark::DoNotOptimize(other);
  }
}

#define BM_SIMPLE_VALUES(type)                             \
  void BM_##type##CopyConstruct(benchmark::State& state) { \
    BM_SimpleCopyConstruct<type, Default##type>(state);    \
  }                                                        \
  BENCHMARK(BM_##type##CopyConstruct);

BM_SIMPLE_VALUES_LIST(BM_SIMPLE_VALUES)

#undef BM_SIMPLE_VALUES

template <typename T, Handle<T> (*F)(ValueFactory&)>
void BM_SimpleMoveConstruct(benchmark::State& state) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  for (auto s : state) {
    Handle<Value> other((*F)(value_factory));
    benchmark::DoNotOptimize(other);
  }
}

#define BM_SIMPLE_VALUES(type)                             \
  void BM_##type##MoveConstruct(benchmark::State& state) { \
    BM_SimpleMoveConstruct<type, Default##type>(state);    \
  }                                                        \
  BENCHMARK(BM_##type##MoveConstruct);

BM_SIMPLE_VALUES_LIST(BM_SIMPLE_VALUES)

}  // namespace
}  // namespace cel