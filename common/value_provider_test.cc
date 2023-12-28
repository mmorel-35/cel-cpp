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

#include <cstdint>
#include <limits>
#include <utility>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using cel::internal::IsOk;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

using ValueProviderTest = common_internal::ThreadCompatibleValueTest<>;

#define VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(element_type)          \
  TEST_P(ValueProviderTest, NewListValueBuilder_##element_type) {         \
    auto list_type = type_factory().CreateListType(element_type());       \
    ASSERT_OK_AND_ASSIGN(auto list_value_builder,                         \
                         value_manager().NewListValueBuilder(list_type)); \
    EXPECT_TRUE(list_value_builder->IsEmpty());                           \
    EXPECT_EQ(list_value_builder->Size(), 0);                             \
    auto list_value = std::move(*list_value_builder).Build();             \
    EXPECT_TRUE(list_value.IsEmpty());                                    \
    EXPECT_EQ(list_value.Size(), 0);                                      \
    EXPECT_EQ(list_value.DebugString(), "[]");                            \
    EXPECT_EQ(list_value.GetType(type_manager()), list_type);             \
  }

VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(BoolType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(BytesType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(DoubleType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(DurationType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(IntType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(ListType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(MapType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(NullType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(OptionalType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(StringType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(TimestampType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(TypeType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(UintType)
VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST(DynType)

#undef VALUE_PROVIDER_NEW_LIST_VALUE_BUILDER_TEST

TEST_P(ValueProviderTest, NewListValueBuilder_ErrorType) {
  EXPECT_THAT(value_manager().NewListValueBuilder(
                  ListType(memory_manager(), ErrorType())),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

#define VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(key_type, value_type)     \
  TEST_P(ValueProviderTest, NewMapValueBuilder_##key_type##_##value_type) { \
    auto map_type = type_factory().CreateMapType(key_type(), value_type()); \
    ASSERT_OK_AND_ASSIGN(auto map_value_builder,                            \
                         value_manager().NewMapValueBuilder(map_type));     \
    EXPECT_TRUE(map_value_builder->IsEmpty());                              \
    EXPECT_EQ(map_value_builder->Size(), 0);                                \
    auto map_value = std::move(*map_value_builder).Build();                 \
    EXPECT_TRUE(map_value.IsEmpty());                                       \
    EXPECT_EQ(map_value.Size(), 0);                                         \
    EXPECT_EQ(map_value.DebugString(), "{}");                               \
    EXPECT_EQ(map_value.GetType(type_manager()), map_type);                 \
  }

VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, BoolType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, BytesType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, DoubleType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, DurationType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, IntType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, ListType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, MapType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, NullType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, OptionalType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, StringType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, TimestampType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, TypeType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, UintType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, DynType)

VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, BoolType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, BytesType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, DoubleType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, DurationType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, IntType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, ListType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, MapType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, NullType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, OptionalType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, StringType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, TimestampType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, TypeType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, UintType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, DynType)

VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, BoolType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, BytesType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, DoubleType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, DurationType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, IntType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, ListType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, MapType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, NullType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, OptionalType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, StringType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, TimestampType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, TypeType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, UintType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, DynType)

VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, BoolType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, BytesType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, DoubleType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, DurationType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, IntType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, ListType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, MapType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, NullType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, OptionalType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, StringType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, TimestampType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, TypeType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, UintType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, DynType)

VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, BoolType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, BytesType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, DoubleType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, DurationType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, IntType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, ListType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, MapType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, NullType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, OptionalType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, StringType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, TimestampType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, TypeType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, UintType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, DynType)

#undef VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST

#define VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(key_type, value_type)     \
  TEST_P(ValueProviderTest, NewMapValueBuilder_##key_type##_##value_type) { \
    EXPECT_THAT(value_manager().NewMapValueBuilder(                         \
                    MapType(memory_manager(), key_type(), value_type())),   \
                StatusIs(absl::StatusCode::kInvalidArgument));              \
  }

VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(BoolType, ErrorType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(IntType, ErrorType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(UintType, ErrorType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(StringType, ErrorType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(DynType, ErrorType)
VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST(ErrorType, ErrorType)

#undef VALUE_PROVIDER_NEW_MAP_VALUE_BUILDER_TEST

TEST_P(ValueProviderTest, NewListValueBuilderCoverage_Dynamic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewListValueBuilder(
                           ListType(type_factory().GetDynListType())));
  EXPECT_OK(builder->Add(IntValue(0)));
  EXPECT_OK(builder->Add(IntValue(1)));
  EXPECT_OK(builder->Add(IntValue(2)));
  EXPECT_EQ(builder->Size(), 3);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(value.DebugString(), "[0, 1, 2]");
}

TEST_P(ValueProviderTest, NewMapValueBuilderCoverage_DynamicDynamic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewMapValueBuilder(
                           type_factory().CreateMapType(DynType(), DynType())));
  EXPECT_OK(builder->Put(BoolValue(false), IntValue(1)));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(2)));
  EXPECT_OK(builder->Put(IntValue(0), IntValue(3)));
  EXPECT_OK(builder->Put(IntValue(1), IntValue(4)));
  EXPECT_OK(builder->Put(UintValue(0), IntValue(5)));
  EXPECT_OK(builder->Put(UintValue(1), IntValue(6)));
  EXPECT_OK(builder->Put(StringValue("a"), IntValue(7)));
  EXPECT_OK(builder->Put(StringValue("b"), IntValue(8)));
  EXPECT_EQ(builder->Size(), 8);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(
      value.DebugString(),
      "{false: 1, true: 2, 0: 3, 1: 4, 0u: 5, 1u: 6, \"a\": 7, \"b\": 8}");
}

TEST_P(ValueProviderTest, NewMapValueBuilderCoverage_StaticDynamic) {
  ASSERT_OK_AND_ASSIGN(
      auto builder, value_manager().NewMapValueBuilder(
                        type_factory().CreateMapType(BoolType(), DynType())));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(0)));
  EXPECT_EQ(builder->Size(), 1);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(value.DebugString(), "{true: 0}");
}

TEST_P(ValueProviderTest, NewMapValueBuilderCoverage_DynamicStatic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewMapValueBuilder(
                           type_factory().CreateMapType(DynType(), IntType())));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(0)));
  EXPECT_EQ(builder->Size(), 1);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(value.DebugString(), "{true: 0}");
}

TEST_P(ValueProviderTest, JsonKeyCoverage) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewMapValueBuilder(MapType(
                                         type_factory().GetDynDynMapType())));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(1)));
  EXPECT_OK(builder->Put(IntValue(1), IntValue(2)));
  EXPECT_OK(builder->Put(UintValue(2), IntValue(3)));
  EXPECT_OK(builder->Put(StringValue("a"), IntValue(4)));
  auto value = std::move(*builder).Build();
  EXPECT_THAT(
      value.ConvertToJson(),
      IsOkAndHolds(Json(MakeJsonObject({{JsonString("true"), Json(1.0)},
                                        {JsonString("1"), Json(2.0)},
                                        {JsonString("2"), Json(3.0)},
                                        {JsonString("a"), Json(4.0)}}))));
}

TEST_P(ValueProviderTest, NewValueBuilder_BoolValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.BoolValue"));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", BoolValue(true)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", IntValue(1)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<BoolValue>(value));
  EXPECT_EQ(Cast<BoolValue>(value).NativeValue(), true);
}

TEST_P(ValueProviderTest, NewValueBuilder_Int32Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Int32Value"));
  EXPECT_THAT(builder->SetFieldByName("value", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName(
                  "value", IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(
                  1, IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<IntValue>(value));
  EXPECT_EQ(Cast<IntValue>(value).NativeValue(), 1);
}

TEST_P(ValueProviderTest, NewValueBuilder_Int64Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Int64Value"));
  EXPECT_THAT(builder->SetFieldByName("value", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<IntValue>(value));
  EXPECT_EQ(Cast<IntValue>(value).NativeValue(), 1);
}

TEST_P(ValueProviderTest, NewValueBuilder_UInt32Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.UInt32Value"));
  EXPECT_THAT(builder->SetFieldByName("value", UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName(
                  "value", UintValue(std::numeric_limits<uint64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(1, UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(
                  1, UintValue(std::numeric_limits<uint64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<UintValue>(value));
  EXPECT_EQ(Cast<UintValue>(value).NativeValue(), 1);
}

TEST_P(ValueProviderTest, NewValueBuilder_UInt64Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.UInt64Value"));
  EXPECT_THAT(builder->SetFieldByName("value", UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<UintValue>(value));
  EXPECT_EQ(Cast<UintValue>(value).NativeValue(), 1);
}

TEST_P(ValueProviderTest, NewValueBuilder_FloatValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.FloatValue"));
  EXPECT_THAT(builder->SetFieldByName("value", DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<DoubleValue>(value));
  EXPECT_EQ(Cast<DoubleValue>(value).NativeValue(), 1);
}

TEST_P(ValueProviderTest, NewValueBuilder_DoubleValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.DoubleValue"));
  EXPECT_THAT(builder->SetFieldByName("value", DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<DoubleValue>(value));
  EXPECT_EQ(Cast<DoubleValue>(value).NativeValue(), 1);
}

TEST_P(ValueProviderTest, NewValueBuilder_StringValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.StringValue"));
  EXPECT_THAT(builder->SetFieldByName("value", StringValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", StringValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, StringValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, StringValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<StringValue>(value));
  EXPECT_EQ(Cast<StringValue>(value).NativeString(), "foo");
}

TEST_P(ValueProviderTest, NewValueBuilder_BytesValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.BytesValue"));
  EXPECT_THAT(builder->SetFieldByName("value", BytesValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", BytesValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, BytesValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, BytesValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<BytesValue>(value));
  EXPECT_EQ(Cast<BytesValue>(value).NativeString(), "foo");
}

TEST_P(ValueProviderTest, NewValueBuilder_Duration) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Duration"));
  EXPECT_THAT(builder->SetFieldByName("seconds", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("seconds", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("nanos", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName(
                  "nanos", IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByName("nanos", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(3, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(
                  2, IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<DurationValue>(value));
  EXPECT_EQ(Cast<DurationValue>(value).NativeValue(),
            absl::Seconds(1) + absl::Nanoseconds(1));
}

TEST_P(ValueProviderTest, NewValueBuilder_Timestamp) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Timestamp"));
  EXPECT_THAT(builder->SetFieldByName("seconds", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("seconds", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("nanos", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName(
                  "nanos", IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByName("nanos", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(3, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(
                  2, IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<TimestampValue>(value));
  EXPECT_EQ(Cast<TimestampValue>(value).NativeValue(),
            absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1));
}

TEST_P(ValueProviderTest, NewValueBuilder_Any) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewValueBuilder("google.protobuf.Any"));
  EXPECT_THAT(builder->SetFieldByName(
                  "type_url",
                  StringValue("type.googleapis.com/google.protobuf.BoolValue")),
              IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("type_url", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("value", BytesValue()), IsOk());
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      builder->SetFieldByNumber(
          1, StringValue("type.googleapis.com/google.protobuf.BoolValue")),
      IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(3, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(2, BytesValue()), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<BoolValue>(value));
  EXPECT_EQ(Cast<BoolValue>(value).NativeValue(), false);
}

INSTANTIATE_TEST_SUITE_P(
    ValueProviderTest, ValueProviderTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ValueProviderTest::ToString);

}  // namespace
}  // namespace cel
