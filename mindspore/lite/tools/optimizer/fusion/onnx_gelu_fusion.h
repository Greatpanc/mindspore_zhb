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

#ifndef MINDSPORE_LITE_TOOLS_OPTIMIZER_FUSION_ONNX_GELU_FUSION_H_
#define MINDSPORE_LITE_TOOLS_OPTIMIZER_FUSION_ONNX_GELU_FUSION_H_

#include <vector>
#include <memory>
#include <string>
#include "tools/optimizer/fusion/gelu_fusion.h"

namespace mindspore {
namespace opt {
class OnnxGeLUFusion : public GeLUFusion {
 public:
  explicit OnnxGeLUFusion(const std::string &name = "onnx_gelu_fusion", bool multigraph = true)
      : GeLUFusion(name, multigraph) {
    div_y_ = std::make_shared<Var>();
    add_y_ = std::make_shared<Var>();
    mul1_y_ = std::make_shared<Var>();
  }
  ~OnnxGeLUFusion() override = default;

 private:
  bool CheckPattern(const EquivPtr &equiv) const override;
  const BaseRef DefinePattern() const override;

 private:
  VarPtr div_y_ = nullptr;
  VarPtr add_y_ = nullptr;
  VarPtr mul1_y_ = nullptr;
};
}  // namespace opt
}  // namespace mindspore

#endif  // MINDSPORE_LITE_TOOLS_OPTIMIZER_FUSION_ONNX_GELU_FUSION_H_
