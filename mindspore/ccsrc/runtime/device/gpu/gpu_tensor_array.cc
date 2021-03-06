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

#include "runtime/device/gpu/gpu_tensor_array.h"
#include <cuda_runtime_api.h>
#include <vector>
#include <string>
#include <memory>
#include "runtime/device/gpu/gpu_common.h"
#include "runtime/device/gpu/gpu_memory_allocator.h"

namespace mindspore {
namespace device {
namespace gpu {
bool GPUTensorArray::CheckValue(const TypeId &dtype, const std::vector<size_t> &shape) {
  MS_LOG(DEBUG) << "Check the data shape and type for " << name_;
  if (dtype != dtype_->type_id()) {
    MS_LOG(ERROR) << "Invalid data type " << TypeIdLabel(dtype) << " for " << name_ << ", the origin type is "
                  << TypeIdLabel(dtype_->type_id());
    return false;
  }
  if (shape != shapes_) {
    MS_LOG(ERROR) << "Invalid data shape " << shape << " for " << name_ << ", the origin shape is " << shapes_;
    return false;
  }
  return true;
}

bool GPUTensorArray::CheckReadIndexLogical(const int64_t index) {
  if (LongToSize(index) >= valid_size_) {
    MS_LOG(ERROR) << "Index " << index << " out of range " << valid_size_ << ", " << name_;
    return false;
  }
  return true;
}

// Add tensor to the TensorArray and increase the size.
// Cast 1: is_dynamic = False and index > max_size_, error.
// Case 2: index > valid_size, fill the rest dev_value with zeros, and set valid_size to index + 1.
// Case 3: index == tensors_.size(), we need to increase both real tensors_ size and valid size, and add
// the new dev_value to tensors_.
// Case 4: tensors_size() > index > valid_size, we can reuse the memory in tensors_[index], so
// only increase the valid_size.
bool GPUTensorArray::Write(const int64_t index, const mindspore::kernel::AddressPtr &dev_value) {
  MS_LOG(DEBUG) << "Write dev_value to " << name_;
  if (!is_dynamic_ && (index >= max_size_)) {
    MS_LOG(ERROR) << name_ << " is not in dynamic size, the max_size is " << max_size_ << ", but get index " << index;
    return false;
  }
  if (LongToSize(index) > valid_size_) {
    // Create/reuse (index - valid_size) size dev_value with zeros.
    // 1 create new mem : index > real_size ? index - real_size : 0
    // 2 reuse old mem : index > real_size ? real_size - valid_size : index - valid_size
    // 3 fill zeros : index - valid_size
    size_t create_size = (LongToSize(index) > tensors_.size()) ? (LongToSize(index) - tensors_.size()) : 0;
    for (size_t i = 0; i < create_size; i++) {
      kernel::AddressPtr create_dev = std::make_shared<kernel::Address>();
      create_dev->addr = device::gpu::GPUMemoryAllocator::GetInstance().AllocTensorMem(dev_value->size);
      create_dev->size = dev_value->size;
      tensors_.push_back(create_dev);
    }
    tensors_.push_back(dev_value);
    // FillZeros(valid_size_, index);
    for (size_t i = valid_size_; i < LongToSize(index); i++) {
      CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(cudaMemsetAsync(tensors_[i]->addr, 0, tensors_[i]->size),
                                         "failed to set cuda memory with zeros.")
    }
    valid_size_ = LongToSize(index) + 1;
  } else if (LongToSize(index) == tensors_.size()) {
    MS_LOG(DEBUG) << "Write to index " << index << ", increase tensors' size to " << (tensors_.size() + 1);
    tensors_.push_back(dev_value);
    valid_size_++;
  } else {
    MS_LOG(DEBUG) << "Reuse tensors in position " << index << ", tensors size is " << tensors_.size();
    if (LongToSize(index) == valid_size_) valid_size_++;
  }
  return true;
}

// Function Read() can get the tensors in the scope of tensors_.
mindspore::kernel::AddressPtr GPUTensorArray::Read(const int64_t index) {
  if (LongToSize(index) >= tensors_.size()) {
    MS_LOG(EXCEPTION) << "Index " << index << " out of range " << tensors_.size() << ", " << name_;
  }
  MS_LOG(DEBUG) << "Read tensor index = " << index << ", addr = " << tensors_[LongToSize(index)]->addr;
  return tensors_[LongToSize(index)];
}

// Free() will free the memory in TensorArray.
void GPUTensorArray::Free() {
  MS_LOG(DEBUG) << "Free device memory for " << name_;
  for (const auto &addr : tensors_) {
    if (addr != nullptr) {
      device::gpu::GPUMemoryAllocator::GetInstance().FreeTensorMem(static_cast<void *>(addr->addr));
    }
  }
}
}  // namespace gpu
}  // namespace device
}  // namespace mindspore
