#include "eval/compiler/qualified_reference_resolver.h"

#include <cstdint>
#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "base/ast.h"
#include "base/internal/ast_impl.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast::Ast;
using ::cel::ast::internal::AstImpl;
using ::cel::ast::internal::Expr;
using ::cel::ast::internal::Reference;
using ::cel::ast::internal::SourceInfo;
using ::cel::extensions::internal::ConvertProtoExprToNative;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::UnorderedElementsAre;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

// foo.bar.var1 && bar.foo.var2
constexpr char kExpr[] = R"(
  id: 1
  call_expr {
    function: "_&&_"
    args {
      id: 2
      select_expr {
        field: "var1"
        operand {
          id: 3
          select_expr {
            field: "bar"
            operand {
              id: 4
              ident_expr { name: "foo" }
            }
          }
        }
      }
    }
    args {
      id: 5
      select_expr {
        field: "var2"
        operand {
          id: 6
          select_expr {
            field: "foo"
            operand {
              id: 7
              ident_expr { name: "bar" }
            }
          }
        }
      }
    }
  }
)";

MATCHER_P(StatusCodeIs, x, "") {
  const absl::Status& status = arg;
  return status.code() == x;
}

std::unique_ptr<AstImpl> ParseTestProto(const std::string& pb) {
  google::api::expr::v1alpha1::Expr expr;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(pb, &expr));
  return absl::WrapUnique(cel::internal::down_cast<AstImpl*>(
      cel::extensions::CreateAstFromParsedExpr(expr).value().release()));
}

TEST(ResolveReferences, Basic) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[5].set_name("bar.foo.var2");
  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);

  auto result = ResolveReferences(registry, warnings, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_&&_"
                                          args {
                                            id: 2
                                            ident_expr { name: "foo.bar.var1" }
                                          }
                                          args {
                                            id: 5
                                            ident_expr { name: "bar.foo.var2" }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, ReturnsFalseIfNoChanges) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);

  auto result = ResolveReferences(registry, warnings, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(false));

  // reference to the same name also doesn't count as a rewrite.
  expr_ast->reference_map()[4].set_name("foo");
  expr_ast->reference_map()[7].set_name("bar");

  result = ResolveReferences(registry, warnings, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(false));
}

TEST(ResolveReferences, NamespacedIdent) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  SourceInfo source_info;
  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[7].set_name("namespace_x.bar");

  auto result = ResolveReferences(registry, warnings, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        id: 1
        call_expr {
          function: "_&&_"
          args {
            id: 2
            ident_expr { name: "foo.bar.var1" }
          }
          args {
            id: 5
            select_expr {
              field: "var2"
              operand {
                id: 6
                select_expr {
                  field: "foo"
                  operand {
                    id: 7
                    ident_expr { name: "namespace_x.bar" }
                  }
                }
              }
            }
          }
        })pb",
      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, WarningOnPresenceTest) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(R"pb(
    id: 1
    select_expr {
      field: "var1"
      test_only: true
      operand {
        id: 2
        select_expr {
          field: "bar"
          operand {
            id: 3
            ident_expr { name: "foo" }
          }
        }
      }
    })pb");
  SourceInfo source_info;

  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[1].set_name("foo.bar.var1");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(
      warnings.warnings(),
      testing::ElementsAre(Eq(absl::Status(
          absl::StatusCode::kInvalidArgument,
          "Reference map points to a presence test -- has(container.attr)"))));
}

// foo.bar.var1 == bar.foo.Enum.ENUM_VAL1
constexpr char kEnumExpr[] = R"(
  id: 1
  call_expr {
    function: "_==_"
    args {
      id: 2
      select_expr {
        field: "var1"
        operand {
          id: 3
          select_expr {
            field: "bar"
            operand {
              id: 4
              ident_expr { name: "foo" }
            }
          }
        }
      }
    }
    args {
      id: 5
      ident_expr { name: "bar.foo.Enum.ENUM_VAL1" }
    }
  }
)";

TEST(ResolveReferences, EnumConstReferenceUsed) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kEnumExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[5].set_name("bar.foo.Enum.ENUM_VAL1");
  expr_ast->reference_map()[5].mutable_value().set_int64_value(9);
  BuilderWarnings warnings;

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_==_"
                                          args {
                                            id: 2
                                            ident_expr { name: "foo.bar.var1" }
                                          }
                                          args {
                                            id: 5
                                            const_expr { int64_value: 9 }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, EnumConstReferenceUsedSelect) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kEnumExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[2].mutable_value().set_int64_value(2);
  expr_ast->reference_map()[5].set_name("bar.foo.Enum.ENUM_VAL1");
  expr_ast->reference_map()[5].mutable_value().set_int64_value(9);
  BuilderWarnings warnings;

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_==_"
                                          args {
                                            id: 2
                                            const_expr { int64_value: 2 }
                                          }
                                          args {
                                            id: 5
                                            const_expr { int64_value: 9 }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, ConstReferenceSkipped) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[2].mutable_value().set_bool_value(true);
  expr_ast->reference_map()[5].set_name("bar.foo.var2");
  BuilderWarnings warnings;

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_&&_"
                                          args {
                                            id: 2
                                            select_expr {
                                              field: "var1"
                                              operand {
                                                id: 3
                                                select_expr {
                                                  field: "bar"
                                                  operand {
                                                    id: 4
                                                    ident_expr { name: "foo" }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                          args {
                                            id: 5
                                            ident_expr { name: "bar.foo.var2" }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

constexpr char kExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  args {
    id: 2
    const_expr {
      bool_value: true
    }
  }
  args {
    id: 3
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceBasic) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(
      CelFunctionDescriptor("boolean_and", false,
                            {
                                CelValue::Type::kBool,
                                CelValue::Type::kBool,
                            })));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  BuilderWarnings warnings;
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
}

TEST(ResolveReferences, FunctionReferenceMissingOverloadDetected) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  BuilderWarnings warnings;
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

TEST(ResolveReferences, SpecialBuiltinsNotWarned) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(R"pb(
    id: 1
    call_expr {
      function: "*"
      args {
        id: 2
        const_expr { bool_value: true }
      }
      args {
        id: 3
        const_expr { bool_value: false }
      }
    })pb");
  SourceInfo source_info;

  std::vector<const char*> special_builtins{builtin::kAnd, builtin::kOr,
                                            builtin::kTernary, builtin::kIndex};
  for (const char* builtin_fn : special_builtins) {
    // Builtins aren't in the function registry.
    CelFunctionRegistry func_registry;
    CelTypeRegistry type_registry;
    Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
    BuilderWarnings warnings;
    expr_ast->reference_map()[1].mutable_overload_id().push_back(
        absl::StrCat("builtin.", builtin_fn));
    expr_ast->root_expr().mutable_call_expr().set_function(builtin_fn);

    auto result = ResolveReferences(registry, warnings, *expr_ast);

    ASSERT_THAT(result, IsOkAndHolds(false));
    EXPECT_THAT(warnings.warnings(), IsEmpty());
  }
}

TEST(ResolveReferences,
     FunctionReferenceMissingOverloadDetectedAndMissingReference) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  BuilderWarnings warnings;
  expr_ast->reference_map()[1].set_name("udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(
      warnings.warnings(),
      UnorderedElementsAre(
          Eq(absl::InvalidArgumentError(
              "No overload found in reference resolve step for boolean_and")),
          Eq(absl::InvalidArgumentError(
              "Reference map doesn't provide overloads for boolean_and"))));
}

TEST(ResolveReferences, EmulatesEagerFailing) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  BuilderWarnings warnings(/*fail_eagerly=*/true);
  expr_ast->reference_map()[1].set_name("udf_boolean_and");

  EXPECT_THAT(
      ResolveReferences(registry, warnings, *expr_ast),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Reference map doesn't provide overloads for boolean_and"));
}

TEST(ResolveReferences, FunctionReferenceToWrongExprKind) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[2].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

constexpr char kReceiverCallExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  target {
    id: 2
    ident_expr {
      name: "ext"
    }
  }
  args {
    id: 3
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceWithTargetNoChange) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "boolean_and", true, {CelValue::Type::kBool, CelValue::Type::kBool})));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

TEST(ResolveReferences,
     FunctionReferenceWithTargetNoChangeMissingOverloadDetected) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

TEST(ResolveReferences, FunctionReferenceWithTargetToNamespacedFunction) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "ext.boolean_and", false, {CelValue::Type::kBool})));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "ext.boolean_and"
                                          args {
                                            id: 3
                                            const_expr { bool_value: false }
                                          }
                                        }
                                      )pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

TEST(ResolveReferences,
     FunctionReferenceWithTargetToNamespacedFunctionInContainer) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");
  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "com.google.ext.boolean_and", false, {CelValue::Type::kBool})));
  CelTypeRegistry type_registry;
  Resolver registry("com.google", func_registry.InternalGetRegistry(),
                    &type_registry);
  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "com.google.ext.boolean_and"
                                          args {
                                            id: 3
                                            const_expr { bool_value: false }
                                          }
                                        }
                                      )pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

// has(ext.option).boolean_and(false)
constexpr char kReceiverCallHasExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  target {
    id: 2
    select_expr {
      test_only: true
      field: "option"
      operand {
        id: 3
        ident_expr {
          name: "ext"
        }
      }
    }
  }
  args {
    id: 4
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceWithHasTargetNoChange) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallHasExtensionAndExpr);
  SourceInfo source_info;

  BuilderWarnings warnings;
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "boolean_and", true, {CelValue::Type::kBool, CelValue::Type::kBool})));
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "ext.option.boolean_and", true, {CelValue::Type::kBool})));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  // The target is unchanged because it is a test_only select.
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(kReceiverCallHasExtensionAndExpr,
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

constexpr char kComprehensionExpr[] = R"(
id:17
comprehension_expr: {
  iter_var:"i"
  iter_range:{
    id:1
    list_expr:{
      elements:{
        id:2
        const_expr:{int64_value:1}
      }
      elements:{
        id:3
        ident_expr:{name:"ENUM"}
      }
      elements:{
        id:4
        const_expr:{int64_value:3}
      }
    }
  }
  accu_var:"__result__"
  accu_init: {
    id:10
    const_expr:{bool_value:false}
  }
  loop_condition:{
    id:13
    call_expr:{
      function:"@not_strictly_false"
      args:{
        id:12
        call_expr:{
          function:"!_"
          args:{
            id:11
            ident_expr:{name:"__result__"}
          }
        }
      }
    }
  }
  loop_step:{
    id:15
    call_expr: {
      function:"_||_"
      args:{
        id:14
        ident_expr: {name:"__result__"}
      }
      args:{
        id:8
        call_expr:{
          function:"_==_"
          args:{
            id:7 ident_expr:{name:"ENUM"}
          }
          args:{
            id:9 ident_expr:{name:"i"}
          }
        }
      }
    }
  }
  result:{id:16 ident_expr:{name:"__result__"}}
}
)";
TEST(ResolveReferences, EnumConstReferenceUsedInComprehension) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kComprehensionExpr);

  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[3].set_name("ENUM");
  expr_ast->reference_map()[3].mutable_value().set_int64_value(2);
  expr_ast->reference_map()[7].set_name("ENUM");
  expr_ast->reference_map()[7].mutable_value().set_int64_value(2);
  BuilderWarnings warnings;

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        id: 17
        comprehension_expr {
          iter_var: "i"
          iter_range {
            id: 1
            list_expr {
              elements {
                id: 2
                const_expr { int64_value: 1 }
              }
              elements {
                id: 3
                const_expr { int64_value: 2 }
              }
              elements {
                id: 4
                const_expr { int64_value: 3 }
              }
            }
          }
          accu_var: "__result__"
          accu_init {
            id: 10
            const_expr { bool_value: false }
          }
          loop_condition {
            id: 13
            call_expr {
              function: "@not_strictly_false"
              args {
                id: 12
                call_expr {
                  function: "!_"
                  args {
                    id: 11
                    ident_expr { name: "__result__" }
                  }
                }
              }
            }
          }
          loop_step {
            id: 15
            call_expr {
              function: "_||_"
              args {
                id: 14
                ident_expr { name: "__result__" }
              }
              args {
                id: 8
                call_expr {
                  function: "_==_"
                  args {
                    id: 7
                    const_expr { int64_value: 2 }
                  }
                  args {
                    id: 9
                    ident_expr { name: "i" }
                  }
                }
              }
            }
          }
          result {
            id: 16
            ident_expr { name: "__result__" }
          }
        })pb",
      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, ReferenceToId0Warns) {
  // ID 0 is unsupported since it is not normally used by parsers and is
  // ambiguous as an intentional ID or default for unset field.
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(R"pb(
    id: 0
    select_expr {
      operand {
        id: 1
        ident_expr { name: "pkg" }
      }
      field: "var"
    })pb");

  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  CelTypeRegistry type_registry;
  Resolver registry("", func_registry.InternalGetRegistry(), &type_registry);
  expr_ast->reference_map()[0].set_name("pkg.var");
  BuilderWarnings warnings;

  auto result = ResolveReferences(registry, warnings, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 0
                                        select_expr {
                                          operand {
                                            id: 1
                                            ident_expr { name: "pkg" }
                                          }
                                          field: "var"
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(
      warnings.warnings(),
      Contains(StatusIs(
          absl::StatusCode::kInvalidArgument,
          "reference map entries for expression id 0 are not supported")));
}
}  // namespace

}  // namespace google::api::expr::runtime