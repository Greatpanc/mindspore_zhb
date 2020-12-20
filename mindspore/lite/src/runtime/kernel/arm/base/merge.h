/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_BASE_MERGE_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_BASE_MERGE_H_

#include <vector>
#include "src/lite_kernel.h"

namespace mindspore::kernel {

typedef struct MergeParameter {
  OpParameter op_parameter_;
} MergeParameter;

class MergeCPUKernel : public LiteKernel {
 public:
  MergeCPUKernel(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                 const std::vector<lite::Tensor *> &outputs, const lite::InnerContext *ctx,
                 const mindspore::lite::PrimitiveC *primitive)
      : LiteKernel(parameter, inputs, outputs, ctx, primitive) {
    merge_param_ = reinterpret_cast<MergeParameter *>(op_parameter_);
  }
  ~MergeCPUKernel() override {}
  int FreeInWorkTensor() const override;
  bool IsReady(const std::vector<lite::Tensor *> &scope_tensors) override;
  int Init() override;
  int ReSize() override;
  int Run() override;
  bool PartialInputReady(int num_begin, int num_end);

 private:
  MergeParameter *merge_param_ = nullptr;
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_BASE_MERGE_H_
