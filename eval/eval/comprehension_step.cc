#include "eval/eval/comprehension_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "base/kind.h"
#include "base/values/unknown_value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"
#include "runtime/internal/mutable_list_impl.h"

namespace google::api::expr::runtime {

using ::cel::Handle;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;
using ::cel::runtime_internal::MutableListValue;

// Stack variables during comprehension evaluation:
// 0. accu_init, then loop_step (any), available through accu_var
// 1. iter_range (list)
// 2. current index in iter_range (int64_t)
// 3. current_value from iter_range (any), available through iter_var
// 4. loop_condition (bool) OR loop_step (any)

// What to put on ExecutionPath:     stack size
//  0. (dummy)                       1
//  1. iter_range              (dep) 2
//  2. -1                            3
//  3. (dummy)                       4
//  4. accu_init               (dep) 5
//  5. ComprehensionNextStep         4
//  6. loop_condition          (dep) 5
//  7. ComprehensionCondStep         4
//  8. loop_step               (dep) 5
//  9. goto 5.                       5
// 10. result                  (dep) 2
// 11. ComprehensionFinish           1

ComprehensionNextStep::ComprehensionNextStep(size_t slot_offset_,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_slot_(slot_offset_ + kComprehensionSlotsIterOffset),
      accu_slot_(slot_offset_ + kComprehensionSlotsAccuOffset) {}

void ComprehensionNextStep::set_jump_offset(int offset) {
  jump_offset_ = offset;
}

void ComprehensionNextStep::set_error_jump_offset(int offset) {
  error_jump_offset_ = offset;
}

// Stack changes of ComprehensionNextStep.
//
// Stack before:
// 0. previous accu_init or "" on the first iteration
// 1. iter_range (list)
// 2. old current_index in iter_range (int64_t)
// 3. old current_value or "" on the first iteration
// 4. loop_step or accu_init (any)
//
// Stack after:
// 0. loop_step or accu_init (any)
// 1. iter_range (list)
// 2. new current_index in iter_range (int64_t)
// 3. new current_value
//
// Stack on break:
// 0. loop_step or accu_init (any)
//
// When iter_range is not a list, this step jumps to error_jump_offset_ that is
// controlled by set_error_jump_offset. In that case the stack is cleared
// from values related to this comprehension and an error is put on the stack.
//
// Stack on error:
// 0. error
absl::Status ComprehensionNextStep::Evaluate(ExecutionFrame* frame) const {
  enum {
    POS_PREVIOUS_LOOP_STEP,
    POS_ITER_RANGE,
    POS_CURRENT_INDEX,
    POS_CURRENT_VALUE,
    POS_LOOP_STEP,
  };
  if (!frame->value_stack().HasEnough(5)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  auto state = frame->value_stack().GetSpan(5);

  // Get range from the stack.
  auto iter_range = state[POS_ITER_RANGE];
  if (!iter_range->Is<cel::ListValue>()) {
    frame->value_stack().Pop(5);
    if (iter_range->Is<cel::ErrorValue>() ||
        iter_range->Is<cel::UnknownValue>()) {
      frame->value_stack().Push(std::move(iter_range));
      return frame->JumpTo(error_jump_offset_);
    }
    frame->value_stack().Push(frame->value_factory().CreateErrorValue(
        CreateNoMatchingOverloadError("<iter_range>")));
    return frame->JumpTo(error_jump_offset_);
  }

  // Get the current index off the stack.
  const auto& current_index_value = state[POS_CURRENT_INDEX];
  if (!current_index_value->Is<cel::IntValue>()) {
    return absl::InternalError(absl::StrCat(
        "ComprehensionNextStep: want int, got ",
        cel::KindToString(ValueKindToKind(current_index_value->kind()))));
  }
  CEL_RETURN_IF_ERROR(frame->IncrementIterations());

  int64_t current_index = current_index_value.As<cel::IntValue>()->value();

  AttributeTrail iter_range_attr;
  AttributeTrail iter_trail;
  if (frame->enable_unknowns()) {
    auto attr = frame->value_stack().GetAttributeSpan(5);
    iter_range_attr = attr[POS_ITER_RANGE];
    iter_trail =
        iter_range_attr.Step(cel::AttributeQualifier::OfInt(current_index + 1));
  }

  // Pop invalidates references to the stack on the following line so copy.
  Handle<Value> loop_step = state[POS_LOOP_STEP];
  frame->value_stack().Pop(5);
  frame->value_stack().Push(loop_step);
  frame->comprehension_slots().Set(accu_slot_, std::move(loop_step));

  // Make sure the iter var is out of scope.
  if (current_index >=
      static_cast<int64_t>(iter_range.As<cel::ListValue>()->size()) - 1) {
    frame->comprehension_slots().ClearSlot(iter_slot_);
    return frame->JumpTo(jump_offset_);
  }
  frame->value_stack().Push(iter_range, std::move(iter_range_attr));
  current_index += 1;

  CEL_ASSIGN_OR_RETURN(
      auto current_value,
      iter_range.As<cel::ListValue>()->Get(frame->value_factory(),
                                           static_cast<size_t>(current_index)));
  frame->value_stack().Push(
      frame->value_factory().CreateIntValue(current_index));
  frame->value_stack().Push(current_value, iter_trail);
  frame->comprehension_slots().Set(iter_slot_, std::move(current_value),
                                   std::move(iter_trail));
  return absl::OkStatus();
}

ComprehensionCondStep::ComprehensionCondStep(size_t slot_offset_,
                                             bool shortcircuiting,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_slot_(slot_offset_ + kComprehensionSlotsIterOffset),
      accu_slot_(slot_offset_ + kComprehensionSlotsAccuOffset),
      shortcircuiting_(shortcircuiting) {}

void ComprehensionCondStep::set_jump_offset(int offset) {
  jump_offset_ = offset;
}

void ComprehensionCondStep::set_error_jump_offset(int offset) {
  error_jump_offset_ = offset;
}

// Stack changes by ComprehensionCondStep.
//
// Stack size before: 5.
// Stack size after: 4.
// Stack size on break: 1.
absl::Status ComprehensionCondStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(5)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  auto loop_condition_value = frame->value_stack().Peek();
  if (!loop_condition_value->Is<cel::BoolValue>()) {
    frame->value_stack().Pop(5);
    if (loop_condition_value->Is<cel::ErrorValue>() ||
        loop_condition_value->Is<cel::UnknownValue>()) {
      frame->value_stack().Push(std::move(loop_condition_value));
    } else {
      frame->value_stack().Push(frame->value_factory().CreateErrorValue(
          CreateNoMatchingOverloadError("<loop_condition>")));
    }
    // The error jump skips the ComprehensionFinish clean-up step, so we
    // need to update the iteration variable stack here.
    frame->comprehension_slots().ClearSlot(iter_slot_);
    frame->comprehension_slots().ClearSlot(accu_slot_);
    return frame->JumpTo(error_jump_offset_);
  }
  bool loop_condition = loop_condition_value.As<cel::BoolValue>()->value();
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    frame->value_stack().Pop(3);  // current_value, current_index, iter_range
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

ComprehensionFinish::ComprehensionFinish(size_t slot_offset_, int64_t expr_id)
    : ExpressionStepBase(expr_id),
      accu_slot_(slot_offset_ + kComprehensionSlotsAccuOffset) {}

// Stack changes of ComprehensionFinish.
//
// Stack size before: 2.
// Stack size after: 1.
absl::Status ComprehensionFinish::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  Handle<Value> result = frame->value_stack().Peek();
  frame->value_stack().Pop(2);
  if (frame->enable_comprehension_list_append() &&
      result->Is<MutableListValue>()) {
    // We assume this is 'owned' by the evaluator stack so const cast is safe
    // here.
    // Convert the buildable list to an actual cel::ListValue.
    MutableListValue& list_value =
        const_cast<MutableListValue&>(result->As<MutableListValue>());
    CEL_ASSIGN_OR_RETURN(result, std::move(list_value).Build());
  }
  frame->value_stack().Push(std::move(result));
  frame->comprehension_slots().ClearSlot(accu_slot_);
  return absl::OkStatus();
}

class ListKeysStep : public ExpressionStepBase {
 public:
  explicit ListKeysStep(int64_t expr_id) : ExpressionStepBase(expr_id, false) {}
  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status ProjectKeys(ExecutionFrame* frame) const;
};

std::unique_ptr<ExpressionStep> CreateListKeysStep(int64_t expr_id) {
  return std::make_unique<ListKeysStep>(expr_id);
}

absl::Status ListKeysStep::ProjectKeys(ExecutionFrame* frame) const {
  // Top of stack is map, but could be partially unknown. To tolerate cases when
  // keys are not set for declared unknown values, convert to an unknown set.
  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            frame->value_stack().GetSpan(1),
            frame->value_stack().GetAttributeSpan(1),
            /*use_partial=*/true);
    if (unknown.has_value()) {
      frame->value_stack().PopAndPush(*std::move(unknown));
      return absl::OkStatus();
    }
  }

  CEL_ASSIGN_OR_RETURN(
      auto list_keys, frame->value_stack().Peek().As<cel::MapValue>()->ListKeys(
                          frame->value_factory()));
  frame->value_stack().PopAndPush(std::move(list_keys));
  return absl::OkStatus();
}

absl::Status ListKeysStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  if (frame->value_stack().Peek()->Is<cel::MapValue>()) {
    return ProjectKeys(frame);
  }
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
