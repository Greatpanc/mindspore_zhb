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

#include "src/runtime/kernel/npu/reshape_npu.h"
#include <memory>
#include "src/kernel_registry.h"
using mindspore::kernel::KERNEL_ARCH::kNPU;
using mindspore::lite::KernelRegistrar;
using mindspore::schema::PrimitiveType_Reshape;

namespace mindspore::kernel {
int ReshapeNPUKernel::IsSupport(const std::vector<lite::Tensor *> &inputs, const std::vector<lite::Tensor *> &outputs,
                                OpParameter *opParameter) {
  if (reshape_param_->shape_dim_ == 0) {
    MS_LOG(ERROR) << "Npu reshape op only supports const shape.";
    return RET_ERROR;
  }
  return RET_OK;
}

int ReshapeNPUKernel::SetNPUInputs(const std::vector<lite::Tensor *> &inputs,
                                   const std::vector<lite::Tensor *> &outputs,
                                   const std::vector<ge::Operator *> &npu_inputs) {
  op_ = new (std::nothrow) hiai::op::Reshape(name_);
  if (op_ == nullptr) {
    MS_LOG(ERROR) << name_ << " op is nullptr";
    return RET_ERROR;
  }
  op_->set_input_x(*npu_inputs[0]);

  shape_op_ = new (std::nothrow) hiai::op::Const(name_ + "_shape");
  std::vector<int> shape;
  for (int i = 0; i < reshape_param_->shape_dim_; i++) {
    shape.push_back(reshape_param_->shape_[i]);
  }
  ge::TensorDesc shape_tensor_desc(ge::Shape({reshape_param_->shape_dim_}), ge::FORMAT_NCHW, ge::DT_INT32);
  ge::TensorPtr ai_shape_tensor = std::make_shared<hiai::Tensor>(shape_tensor_desc);
  ai_shape_tensor->SetData(reinterpret_cast<uint8_t *>(shape.data()), reshape_param_->shape_dim_ * sizeof(int32_t));
  shape_op_->set_attr_value(ai_shape_tensor);
  op_->set_input_shape(*shape_op_);
  return RET_OK;
}

ge::Operator *mindspore::kernel::ReshapeNPUKernel::GetNPUOp() { return this->op_; }

ReshapeNPUKernel::~ReshapeNPUKernel() {
  if (op_ != nullptr) {
    delete op_;
    op_ = nullptr;
  }
  if (shape_op_ != nullptr) {
    delete shape_op_;
    shape_op_ = nullptr;
  }
}

REG_KERNEL(kNPU, kNumberTypeFloat32, PrimitiveType_Reshape, NPUKernelCreator<ReshapeNPUKernel>)
}  // namespace mindspore::kernel
