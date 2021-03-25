/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/optimizer/fusion/tf_gelu_fusion.h"
#include "src/ops/activation.h"

namespace mindspore {
namespace opt {
namespace {
constexpr float DIFF_THRESHOLD = 0.0001;
constexpr float POW_Y = 3;
constexpr float MUL1_Y = 0.044715;
constexpr float MUL2_X = 0.79788;
constexpr float ADD2_X = 1.0;
constexpr float MUL3_X = 0.5;
bool CheckTanh(const EquivPtr &equiv, const VarPtr &input) {
  MS_ASSERT(equiv != nullptr);
  MS_ASSERT(input != nullptr);
  auto anf_node = utils::cast<AnfNodePtr>((*equiv)[input]);
  MS_ASSERT(anf_node != nullptr);
  AnfNodePtr value_node = anf_node;
  if (anf_node->isa<CNode>()) {
    value_node = anf_node->cast<CNodePtr>()->input(0);
  }
  auto act_prim_c = GetValueNode<std::shared_ptr<lite::Activation>>(value_node);
  if (act_prim_c == nullptr) {
    return false;
  }
  auto act_prim = act_prim_c->primitiveT();
  MS_ASSERT(act_prim != nullptr);
  auto attr_value = act_prim->value.value;
  if (attr_value == nullptr) {
    return false;
  }
  auto attr = static_cast<schema::ActivationT *>(attr_value);
  return attr->type == schema::ActivationType_TANH;
}
}  // namespace

// gelu(x) = 1/2 * x * [1 + tanh(0.79788 * (x + 0.044715 * x ^ 3))]
const BaseRef TfGeLUFusion::DefinePattern() const {
  VectorRef pow_ref({power_, input_, power_y_});
  VectorRef mul1_ref({std::make_shared<CondVar>(IsSpecifiedNode<schema::PrimitiveType_Mul>), mul1_x_, pow_ref});
  VectorRef add1_ref({std::make_shared<CondVar>(IsSpecifiedNode<schema::PrimitiveType_Add>), input_, mul1_ref});
  VectorRef mul2_ref({std::make_shared<CondVar>(IsSpecifiedNode<schema::PrimitiveType_Mul>), mul2_x_, add1_ref});
  VectorRef tanh_ref({tanh_, mul2_ref});
  VectorRef add2_ref({std::make_shared<CondVar>(IsSpecifiedNode<schema::PrimitiveType_Add>), add2_x_, tanh_ref});
  VectorRef mul3_ref({std::make_shared<CondVar>(IsSpecifiedNode<schema::PrimitiveType_Mul>), mul3_x_, add2_ref});
  VectorRef mul4_ref({std::make_shared<CondVar>(IsSpecifiedNode<schema::PrimitiveType_Mul>), input_, mul3_ref});
  return mul4_ref;
}

bool TfGeLUFusion::CheckPattern(const EquivPtr &equiv) const {
  MS_ASSERT(equiv != nullptr);
  if (!CheckTanh(equiv, tanh_)) {
    return false;
  }
  float pow_y = GetParameterValue(equiv, power_y_);
  if (pow_y < 0 || fabs(pow_y - POW_Y) > DIFF_THRESHOLD) {
    return false;
  }
  float mul1_y = GetParameterValue(equiv, mul1_x_);
  if (mul1_y < 0 || fabs(mul1_y - MUL1_Y) > DIFF_THRESHOLD) {
    return false;
  }
  float mul2_x = GetParameterValue(equiv, mul2_x_);
  if (mul2_x < 0 || fabs(mul2_x - MUL2_X) > DIFF_THRESHOLD) {
    return false;
  }
  float add2_x = GetParameterValue(equiv, add2_x_);
  if (add2_x < 0 || fabs(add2_x - ADD2_X) > DIFF_THRESHOLD) {
    return false;
  }
  float mul3_x = GetParameterValue(equiv, mul3_x_);
  if (mul3_x < 0 || fabs(mul3_x - MUL3_X) > DIFF_THRESHOLD) {
    return false;
  }
  return true;
}
}  // namespace opt
}  // namespace mindspore
