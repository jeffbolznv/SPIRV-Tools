// Copyright (c) 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/fuzz/transformation_add_local_variable.h"

#include "source/fuzz/fuzzer_util.h"

namespace spvtools {
namespace fuzz {

TransformationAddLocalVariable::TransformationAddLocalVariable(
    const spvtools::fuzz::protobufs::TransformationAddLocalVariable& message)
    : message_(message) {}

TransformationAddLocalVariable::TransformationAddLocalVariable(
    uint32_t fresh_id, uint32_t type_id, uint32_t function_id,
    uint32_t initializer_id, bool value_is_arbitrary) {
  message_.set_fresh_id(fresh_id);
  message_.set_type_id(type_id);
  message_.set_function_id(function_id);
  message_.set_initializer_id(initializer_id);
  message_.set_value_is_arbitrary(value_is_arbitrary);
}

bool TransformationAddLocalVariable::IsApplicable(
    opt::IRContext* context,
    const spvtools::fuzz::FactManager& /*unused*/) const {
  // The provided id must be fresh.
  if (!fuzzerutil::IsFreshId(context, message_.fresh_id())) {
    return false;
  }
  // The pointer type id must indeed correspond to a pointer, and it must have
  // function storage class.
  auto type_instruction =
      context->get_def_use_mgr()->GetDef(message_.type_id());
  if (!type_instruction || type_instruction->opcode() != SpvOpTypePointer ||
      type_instruction->GetSingleWordInOperand(0) != SpvStorageClassFunction) {
    return false;
  }
  // The initializer must...
  auto initializer_instruction =
      context->get_def_use_mgr()->GetDef(message_.initializer_id());
  // ... exist, ...
  if (!initializer_instruction) {
    return false;
  }
  // ... be a constant, ...
  if (!spvOpcodeIsConstant(initializer_instruction->opcode())) {
    return false;
  }
  // ... and have the same type as the pointee type.
  if (initializer_instruction->type_id() !=
      type_instruction->GetSingleWordInOperand(1)) {
    return false;
  }
  // The function to which the local variable is to be added must exist.
  return fuzzerutil::FindFunction(context, message_.function_id());
}

void TransformationAddLocalVariable::Apply(
    opt::IRContext* context, spvtools::fuzz::FactManager* fact_manager) const {
  fuzzerutil::UpdateModuleIdBound(context, message_.fresh_id());
  fuzzerutil::FindFunction(context, message_.function_id())
      ->begin()
      ->begin()
      ->InsertBefore(MakeUnique<opt::Instruction>(
          context, SpvOpVariable, message_.type_id(), message_.fresh_id(),
          opt::Instruction::OperandList(
              {{SPV_OPERAND_TYPE_STORAGE_CLASS,
                {

                    SpvStorageClassFunction}},
               {SPV_OPERAND_TYPE_ID, {message_.initializer_id()}}})));
  if (message_.value_is_arbitrary()) {
    fact_manager->AddFactValueOfVariableIsArbitrary(message_.fresh_id());
  }
  context->InvalidateAnalysesExceptFor(opt::IRContext::kAnalysisNone);
}

protobufs::Transformation TransformationAddLocalVariable::ToMessage() const {
  protobufs::Transformation result;
  *result.mutable_add_local_variable() = message_;
  return result;
}

}  // namespace fuzz
}  // namespace spvtools