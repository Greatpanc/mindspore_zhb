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

#include "runtime/framework/actor/control_flow/stack_actor.h"
#include "runtime/framework/actor/memory_manager_actor.h"
#include "runtime/framework/control_node_parser.h"

namespace mindspore {
namespace runtime {
StackActor::StackActor(const std::string &name, const std::vector<KernelWithIndex> &parameters)
    : ControlActor(name, KernelTransformType::kStackActor, parameters, nullptr) {
  input_device_tensors_.resize(parameters.size());
}

void StackActor::Init() {
  ControlActor::Init();
  // The stack actor has 6 parts of input :
  // 1. Directly input data.
  // 2. Direct input partial.
  // 3. Weight.
  // 4. Local tensor.
  // 5. Call input data.
  // 6. Call input partial.
  input_datas_num_ = formal_parameters_.size() - input_parameter_data_num_ - input_parameter_partial_num_;
  if (input_parameter_data_num_ < device_tensor_store_keys_.size() + local_device_tensors_.size()) {
    MS_LOG(EXCEPTION) << "Invalid input parameter data num:" << input_parameter_data_num_
                      << " device store num:" << device_tensor_store_keys_.size() << " local device tensor num"
                      << local_device_tensors_.size() << " for actor:" << GetAID();
  }

  // Fetch the total number of input partial.
  int total_partials_num = 0;
  for (const auto &formal_parameter : formal_parameters_) {
    const auto &abstract = formal_parameter.first->abstract();
    MS_EXCEPTION_IF_NULL(abstract);
    const auto &real_abstract = FetchAbstractByIndex(abstract, formal_parameter.second);
    MS_EXCEPTION_IF_NULL(real_abstract);
    if (real_abstract->isa<abstract::AbstractFunction>()) {
      total_partials_num++;
    }
  }

  // Fetch call input data num.
  input_datas_num_ = formal_parameters_.size() - total_partials_num - input_parameter_data_num_;
  input_partials_num_ = total_partials_num - input_parameter_partial_num_;
  // Fetch call input partial num.
  input_parameter_data_num_ -= (device_tensor_store_keys_.size() + local_device_tensors_.size());
  // Check if the input num is valid.
  if (input_parameter_data_num_ + input_parameter_partial_num_ + input_datas_num_ + input_partials_num_ +
        device_tensor_store_keys_.size() + local_device_tensors_.size() !=
      formal_parameters_.size()) {
    MS_LOG(EXCEPTION) << "Invalid input num, input parameter data num:" << input_parameter_data_num_
                      << " input parameter partial num:" << input_parameter_partial_num_
                      << " input data num:" << input_datas_num_ << " input partial num:" << input_partials_num_
                      << " device tensor store size:" << device_tensor_store_keys_.size()
                      << " need total size:" << formal_parameters_.size() << " for actor:" << GetAID();
  }
}

void StackActor::RunOpData(OpData<DeviceTensor> *const input_data, OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(context);
  MS_EXCEPTION_IF_NULL(input_data);
  MS_EXCEPTION_IF_NULL(input_data->data_);
  // The parameters from the inside of the subgraph need to be put into the stack.
  if (IntToSize(input_data->index_) < input_parameter_data_num_ + device_tensor_store_keys_.size() +
                                        input_parameter_partial_num_ + local_device_tensors_.size()) {
    FillStack(input_data, context);
  } else {
    // The outputs of call nodes are placed directly in the input data.
    input_op_datas_[context->sequential_num_].emplace_back(input_data);
  }
  if (CheckRunningCondition(context)) {
    Run(context);
  }
}

void StackActor::RunOpPartial(FuncGraph *func_graph, std::vector<DeviceTensor *> input_data, size_t position,
                              OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(context);
  MS_EXCEPTION_IF_NULL(func_graph);
  // The parameters from the inside of the subgraph need to be put into the stack.
  if (IntToSize(position) < input_parameter_data_num_ + device_tensor_store_keys_.size() +
                              input_parameter_partial_num_ + local_device_tensors_.size()) {
    input_parameter_partial_[context->sequential_num_][position].push(OpPartial(func_graph, input_data));
  } else {
    input_op_partials_[context->sequential_num_].emplace_back(position, OpPartial(func_graph, input_data));
  }
  if (CheckRunningCondition(context)) {
    Run(context);
  }
}

void StackActor::FillStack(OpData<DeviceTensor> *const input_data, OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(context);
  MS_EXCEPTION_IF_NULL(input_data);
  auto &input_device_tensor = input_data->data_;
  MS_EXCEPTION_IF_NULL(input_device_tensor);
  auto &sequential_num = context->sequential_num_;
  size_t index = IntToSize(input_data->index_);
  if (index >= device_contexts_.size()) {
    SET_OPCONTEXT_FAIL_RET_WITH_ERROR(*context, "The index is out of range.");
  }

  // 1.If device context is empty, it means that the input is from a parameter and does not need copy new device tensor.
  // 2.If the address ptr can be changed, it has been copied by exit actor and does not need copy a new device tensor.
  if ((device_contexts_[index] == nullptr) || (!input_device_tensor->is_ptr_persisted())) {
    input_parameter_data_[sequential_num][input_data->index_].push(input_device_tensor);
  } else {
    const KernelWithIndex &node_with_index = input_device_tensor->GetNodeIndex();
    MS_EXCEPTION_IF_NULL(node_with_index.first);
    // Create the new device tensor and copy the data from the input data.
    auto new_device_tensor = device_contexts_[index]->CreateDeviceAddress(
      nullptr, input_device_tensor->GetSize(), input_device_tensor->format(), input_device_tensor->type_id());
    MS_EXCEPTION_IF_NULL(new_device_tensor);

    if (!device_contexts_[index]->AllocateMemory(new_device_tensor.get(), new_device_tensor->GetSize())) {
      SET_OPCONTEXT_MEMORY_ALLOC_FAIL_BY_STRATEGY(GraphExecutionStrategy::kPipeline, *context, *device_contexts_[index],
                                                  GetAID().Name(), new_device_tensor->GetSize());
    }
    if (!new_device_tensor->SyncDeviceToDevice(
          trans::GetRuntimePaddingShape(node_with_index.first, node_with_index.second), input_device_tensor->GetSize(),
          input_device_tensor->type_id(), input_device_tensor->GetPtr(), input_device_tensor->format())) {
      SET_OPCONTEXT_FAIL_RET_WITH_ERROR(*context, "Sync device to device failed.");
    }
    new_device_tensor->SetNodeIndex(node_with_index.first, node_with_index.second);
    new_device_tensor->set_original_ref_count(SIZE_MAX);
    new_device_tensor->ResetRefCount();

    created_device_tensors_.emplace_back(new_device_tensor);
    input_parameter_data_[sequential_num][input_data->index_].push(new_device_tensor.get());
  }
}

bool StackActor::CheckRunningCondition(const OpContext<DeviceTensor> *context) const {
  MS_EXCEPTION_IF_NULL(context);
  if (!ControlActor::CheckRunningCondition(context)) {
    return false;
  }

  if (input_parameter_data_num_ != 0) {
    const auto &data_iter = input_parameter_data_.find(context->sequential_num_);
    if (data_iter == input_parameter_data_.end()) {
      return false;
    }
    if (data_iter->second.size() != input_parameter_data_num_) {
      return false;
    }

    auto iter = input_branch_ids_.find(context->sequential_num_);
    if (iter == input_branch_ids_.end() || iter->second.empty()) {
      MS_LOG(ERROR) << "There is no branch id for actor:" << GetAID();
    }
    size_t branch_id_size = iter->second.size();
    if (std::any_of(data_iter->second.begin(), data_iter->second.end(),
                    [branch_id_size](const auto &one_stack) { return one_stack.second.size() != branch_id_size; })) {
      return false;
    }
  }

  if (input_parameter_partial_num_ != 0) {
    const auto &partial_iter = input_parameter_partial_.find(context->sequential_num_);
    if (partial_iter == input_parameter_partial_.end()) {
      return false;
    }
    if (partial_iter->second.size() != input_parameter_partial_num_) {
      return false;
    }

    auto iter = input_branch_ids_.find(context->sequential_num_);
    if (iter == input_branch_ids_.end() || iter->second.empty()) {
      MS_LOG(ERROR) << "There is no branch id for actor:" << GetAID();
    }
    size_t branch_id_size = iter->second.size();
    if (std::any_of(partial_iter->second.begin(), partial_iter->second.end(),
                    [branch_id_size](const auto &one_stack) { return one_stack.second.size() != branch_id_size; })) {
      return false;
    }
  }
  return true;
}

void StackActor::FetchInput(OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(context);
  if (input_parameter_data_num_ != 0) {
    const auto &data_iter = input_parameter_data_.find(context->sequential_num_);
    if (data_iter == input_parameter_data_.end()) {
      MS_LOG(ERROR) << "Invalid input for actor:" << GetAID();
    }
    for (const auto &one_stack : data_iter->second) {
      if (one_stack.first >= input_parameter_data_num_ + device_tensor_store_keys_.size() +
                               local_device_tensors_.size() + input_parameter_partial_num_) {
        MS_LOG(ERROR) << "Invalid input index:" << one_stack.first << " need:" << input_parameter_data_num_
                      << " for actor:" << GetAID();
      }
      input_device_tensors_[one_stack.first] = one_stack.second.top();
    }
  }

  if (input_parameter_partial_num_ != 0) {
    const auto &partial_iter = input_parameter_partial_.find(context->sequential_num_);
    if (partial_iter == input_parameter_partial_.end()) {
      MS_LOG(ERROR) << "Invalid input for actor:" << GetAID();
    }
    for (const auto &one_stack : partial_iter->second) {
      if (one_stack.first >= input_parameter_data_num_ + device_tensor_store_keys_.size() +
                               local_device_tensors_.size() + input_parameter_partial_num_) {
        MS_LOG(ERROR) << "Invalid input index:" << one_stack.first << " need:" << input_parameter_partial_
                      << " for actor:" << GetAID();
      }
      input_partials_[one_stack.first] = one_stack.second.top();
    }
  }
  ControlActor::FetchInput(context);
}

void StackActor::EraseInput(const OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(context);
  ControlActor::EraseInput(context);

  if (input_parameter_data_num_ != 0) {
    const auto &data_iter = input_parameter_data_.find(context->sequential_num_);
    if (data_iter == input_parameter_data_.end()) {
      MS_LOG(ERROR) << "Invalid input for actor:" << GetAID();
    }

    for (auto &one_stack : data_iter->second) {
      if (one_stack.second.empty()) {
        MS_LOG(ERROR) << "Input index:" << one_stack.first << " is null in actor:" << GetAID();
      }
      one_stack.second.pop();
    }
  }

  if (input_parameter_partial_num_ != 0) {
    const auto &partial_iter = input_parameter_partial_.find(context->sequential_num_);
    if (partial_iter == input_parameter_partial_.end()) {
      MS_LOG(ERROR) << "Invalid input for actor:" << GetAID();
    }

    for (auto &one_stack : partial_iter->second) {
      if (one_stack.second.empty()) {
        MS_LOG(ERROR) << "Input index:" << one_stack.first << " is null in actor:" << GetAID();
      }
      one_stack.second.pop();
    }
  }
}
}  // namespace runtime
}  // namespace mindspore
