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

#include "tools/optimizer/fusion/gelu_fusion.h"
#include <memory>
#include <string>
#include "utils/utils.h"
#include "src/ops/primitive_c.h"
#include "tools/optimizer/common/gllo_utils.h"

namespace mindspore {
namespace opt {
CNodePtr GeLUFusion::CreateGeLUNode(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                    const EquivPtr &equiv) const {
  MS_ASSERT(func_graph != nullptr);
  MS_ASSERT(node != nullptr);
  auto gelu_prim = std::make_unique<schema::PrimitiveT>();
  auto attr = std::make_unique<schema::GeLUT>();
  gelu_prim->value.type = schema::PrimitiveType_GeLU;
  gelu_prim->value.value = attr.release();
  auto gelu_prim_c = std::shared_ptr<lite::PrimitiveC>(lite::PrimitiveC::Create(gelu_prim.release()));
  auto input_node = utils::cast<AnfNodePtr>((*equiv)[input_]);
  MS_ASSERT(input_node != nullptr);
  auto gelu_cnode = func_graph->NewCNode(gelu_prim_c, {input_node});
  gelu_cnode->set_fullname_with_scope(node->fullname_with_scope() + "_gelu");
  gelu_cnode->set_abstract(node->abstract()->Clone());
  return gelu_cnode;
}

const float GeLUFusion::GetParameterValue(const EquivPtr &equiv, const VarPtr &input) const {
  MS_ASSERT(equiv != nullptr);
  MS_ASSERT(input != nullptr);
  float value = -1;
  auto node = utils::cast<AnfNodePtr>((*equiv)[input]);
  if (node == nullptr || !utils::isa<ParameterPtr>(node)) {
    return value;
  }
  auto parameter_node = node->cast<ParameterPtr>();
  if (!parameter_node->has_default() || parameter_node->default_param() == nullptr) {
    return value;
  }
  auto param_value_lite = parameter_node->default_param()->cast<ParamValueLitePtr>();
  if (param_value_lite == nullptr) {
    return value;
  }
  if (param_value_lite->tensor_type() != kNumberTypeFloat32 && param_value_lite->tensor_type() != kNumberTypeFloat) {
    return value;
  }
  if (param_value_lite->tensor_size() != sizeof(float)) {
    return value;
  }
  return *static_cast<float *>(param_value_lite->tensor_addr());
}

const AnfNodePtr GeLUFusion::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                     const EquivPtr &equiv) const {
  MS_ASSERT(func_graph != nullptr);
  MS_ASSERT(node != nullptr);
  MS_ASSERT(equiv != nullptr);
  MS_LOG(DEBUG) << "gelu_fusion pass";
  if (!utils::isa<CNodePtr>(node)) {
    return nullptr;
  }
  if (!CheckPattern(equiv)) {
    return nullptr;
  }
  auto cnode = CreateGeLUNode(func_graph, node, equiv);
  if (cnode == nullptr) {
    MS_LOG(DEBUG) << "new gelu node failed.";
    return nullptr;
  }
  return cnode;
}
}  // namespace opt
}  // namespace mindspore
