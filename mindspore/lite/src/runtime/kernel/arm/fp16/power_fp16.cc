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

#include "src/runtime/kernel/arm/fp16/power_fp16.h"
#include "schema/model_generated.h"
#include "src/kernel_registry.h"
#include "src/runtime/runtime_api.h"
#include "include/errorcode.h"

using mindspore::lite::KernelRegistrar;
using mindspore::lite::RET_ERROR;
using mindspore::lite::RET_OK;
using mindspore::schema::PrimitiveType_Power;

namespace mindspore::kernel {
int PowerFp16CPUKernel::Init() { return RET_OK; }

int PowerFp16CPUKernel::ReSize() { return RET_OK; }

int PowerImplFp16(void *cdata, int task_id) {
  auto kernel = reinterpret_cast<PowerFp16CPUKernel *>(cdata);
  auto ret = kernel->RunImpl(task_id);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "PowerFp16Impl error: " << ret;
    return ret;
  }
  return RET_OK;
}

int PowerFp16CPUKernel::Run() {
  auto ret = ParallelLaunch(this->context_->thread_pool_, PowerImplFp16, this, thread_count_);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "PowerFp16CPUKernel error: " << ret;
    return RET_ERROR;
  }
  return RET_OK;
}

int PowerFp16CPUKernel::RunImpl(int task_id) {
  auto x_addr = reinterpret_cast<float16_t *>(in_tensors_.at(0)->MutableData());
  MS_ASSERT(x_addr);
  auto output_addr = reinterpret_cast<float16_t *>(out_tensors_.at(0)->MutableData());
  MS_ASSERT(output_addr);
  auto size = in_tensors_.at(0)->ElementsNum();
  int stride = UP_DIV(size, thread_count_);
  int len = MSMIN(stride, size - stride * task_id);
  if (len <= 0) {
    return RET_OK;
  }
  float16_t *exp_addr = nullptr;
  bool broadcast = true;
  if (in_tensors_.size() == 2) {
    exp_addr = reinterpret_cast<float16_t *>(in_tensors_[1]->MutableData());
    MS_ASSERT(exp_addr);
    broadcast = in_tensors_[0]->shape() == in_tensors_[1]->shape() ? false : true;
  }
  float16_t *cur_exp = nullptr;
  if (broadcast) {
    cur_exp = in_tensors_.size() == 2 ? exp_addr : &power_;
  } else {
    cur_exp = exp_addr + stride * task_id;
  }
  PowerFp16(x_addr + stride * task_id, cur_exp, output_addr + stride * task_id, len, scale_, shift_, broadcast);
  return RET_OK;
}

REG_KERNEL(kCPU, kNumberTypeFloat16, PrimitiveType_Power, LiteKernelCreator<PowerFp16CPUKernel>)
}  // namespace mindspore::kernel
