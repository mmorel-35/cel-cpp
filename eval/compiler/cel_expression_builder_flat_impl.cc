/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/compiler/cel_expression_builder_flat_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/macros.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "eval/eval/evaluator_core.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

using ::cel::ast::Ast;
using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;  // NOLINT: adjusted in OSS
using ::google::api::expr::v1alpha1::SourceInfo;

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const Expr* expr, const SourceInfo* source_info,
    std::vector<absl::Status>* warnings) const {
  ABSL_ASSERT(expr != nullptr);
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<Ast> converted_ast,
      cel::extensions::CreateAstFromParsedExpr(*expr, source_info));
  return flat_expr_builder_.CreateExpressionImpl(std::move(converted_ast),
                                                 warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const Expr* expr, const SourceInfo* source_info) const {
  return CreateExpression(expr, source_info,
                          /*warnings=*/nullptr);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const CheckedExpr* checked_expr,
    std::vector<absl::Status>* warnings) const {
  ABSL_ASSERT(checked_expr != nullptr);
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<Ast> converted_ast,
      cel::extensions::CreateAstFromCheckedExpr(*checked_expr));
  return flat_expr_builder_.CreateExpressionImpl(std::move(converted_ast),
                                                 warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const CheckedExpr* checked_expr) const {
  return CreateExpression(checked_expr, /*warnings=*/nullptr);
}

}  // namespace google::api::expr::runtime