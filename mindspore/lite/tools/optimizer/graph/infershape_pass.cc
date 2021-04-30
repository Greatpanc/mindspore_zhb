/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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
#include "tools/optimizer/graph/infershape_pass.h"
#include <vector>
#include <memory>
#include <algorithm>
#include "include/errorcode.h"
#include "tools/common/node_util.h"
#include "tools/common/tensor_util.h"
#include "src/common/common.h"
#include "src/ops/populate/populate_register.h"
#include "src/ops/ops_utils.h"
#include "src/runtime/infer_manager.h"

namespace mindspore::opt {
namespace {
constexpr size_t INITIAL_SIZE = 1024;
tensor::TensorPtr NewTensorInfo(lite::Tensor *tensor) {
  std::vector<int> shape(tensor->shape());
  std::vector<int64_t> shape_vector;
  std::transform(shape.begin(), shape.end(), std::back_inserter(shape_vector),
                 [](const int32_t &value) { return static_cast<int64_t>(value); });
  auto tensor_info = std::make_shared<tensor::Tensor>(tensor->data_type(), shape_vector);
  if (tensor_info == nullptr) {
    MS_LOG(ERROR) << "new tensor::Tensor failed";
    return nullptr;
  }
  return tensor_info;
}

bool IsSpecialType(const CNodePtr &cnode) {
  if (CheckPrimitiveType(cnode, prim::kPrimTupleGetItem) || CheckPrimitiveType(cnode, prim::kPrimDepend) ||
      CheckPrimitiveType(cnode, prim::kPrimMakeTuple) || CheckPrimitiveType(cnode, prim::kPrimReturn) ||
      CheckPrimitiveType(cnode, std::make_shared<Primitive>("While")) ||
      CheckPrimitiveType(cnode, std::make_shared<Primitive>("If"))) {
    return true;
  }
  return false;
}

STATUS GetTensorInfoFromAbstract(tensor::TensorPtr *tensor_info, const CNodePtr &cnode, size_t index) {
  AbstractBasePtr abstract = GetCNodeInputAbstract(cnode, index);
  if (abstract == nullptr) {
    MS_LOG(ERROR) << "Abstract of CNode: " << cnode->fullname_with_scope() << " is nullptr";
    return RET_ERROR;
  }
  if (!utils::isa<abstract::AbstractTensorPtr>(abstract)) {
    MS_LOG(DEBUG) << "Abstract of parameter should be abstract tensor";
    return RET_ERROR;
  }
  auto abstract_tensor = utils::cast<abstract::AbstractTensorPtr>(abstract);
  if (!utils::isa<tensor::TensorPtr>(abstract_tensor->GetValueTrack())) {  // input node not complete infershape
    MS_LOG(DEBUG) << "Value of abstract is not tensor::Tensor, indicate that infershape has failed";
    return RET_ERROR;
  }
  *tensor_info = utils::cast<tensor::TensorPtr>(abstract_tensor->GetValueTrack());
  if (*tensor_info == nullptr) {
    MS_LOG(ERROR) << "tensor::Tensor of abstract is nullptr";
    return RET_ERROR;
  }
  return RET_OK;
}
}  // namespace

abstract::AbstractTensorPtr InferShapePass::ConvertLiteTensorToAbstractTensor(lite::Tensor *tensor) {
  MS_ASSERT(nullptr != tensor);
  std::vector<int> shape(tensor->shape());
  auto type_id = static_cast<TypeId>(tensor->data_type());
  auto type_ptr = TypeIdToType(type_id);
  std::vector<int64_t> shape_vector(shape.begin(), shape.end());
  auto new_abstract = std::make_shared<abstract::AbstractTensor>(type_ptr, shape_vector);
  if (new_abstract == nullptr) {
    MS_LOG(ERROR) << "new AbstractTensor failed";
    return nullptr;
  }

  auto tensor_info = NewTensorInfo(tensor);
  if (tensor_info == nullptr) {
    MS_LOG(ERROR) << "new tensor::Tensor failed";
    return nullptr;
  }

  if (type_id == kObjectTypeTensorType) {
    auto tensor_list = dynamic_cast<lite::TensorList *>(tensor);
    if (tensor_list == nullptr) {
      MS_LOG(ERROR) << "cast tensor_list failed";
      return nullptr;
    }
    auto tensor_data = new int[tensor_list->element_shape().size() + 2];
    tensor_data[0] = tensor_list->tensors_data_type();
    tensor_data[1] = tensor_list->element_shape().size();
    for (size_t i = 0; i < tensor_list->element_shape().size(); ++i) {
      tensor_data[i + 2] = tensor_list->element_shape()[i];
    }
    auto status = lite::SetTensorData(tensor_info, tensor_data, tensor_list->element_shape().size() + 2);
    delete[] tensor_data;
    if (status != RET_OK) {
      MS_LOG(ERROR) << "set tensor data failed";
      return nullptr;
    }
  }
  new_abstract->set_value(tensor_info);
  return new_abstract;
}

STATUS InferShapePass::SetParameterAbstract(const ParameterPtr &parameter) {
  MS_ASSERT(parameter != nullptr);
  auto old_abstract = parameter->abstract();
  if (old_abstract == nullptr) {
    MS_LOG(ERROR) << "Abstract of parameter is nullptr, " << parameter->name();
    return RET_ERROR;
  }
  if (!utils::isa<abstract::AbstractTensorPtr>(old_abstract)) {
    MS_LOG(ERROR) << "Abstract of parameter should be abstract tensor, " << parameter->name();
    return RET_ERROR;
  }
  auto abstract_tensor = utils::cast<abstract::AbstractTensorPtr>(old_abstract);

  auto type_ptr = abstract_tensor->element()->GetTypeTrack();
  if (type_ptr == nullptr) {
    MS_LOG(ERROR) << "type_ptr is nullptr";
    return RET_ERROR;
  }

  if (!utils::isa<abstract::ShapePtr>(abstract_tensor->BuildShape())) {
    MS_LOG(ERROR) << "Shape of Abstract of parameter should be ShapePtr, " << parameter->name();
    return RET_ERROR;
  }
  auto shape_vector = utils::cast<abstract::ShapePtr>(abstract_tensor->BuildShape())->shape();
  std::vector<int32_t> shape;
  (void)std::transform(shape_vector.begin(), shape_vector.end(), std::back_inserter(shape),
                       [](const int64_t &value) { return static_cast<int32_t>(value); });

  auto new_abstract = std::make_shared<abstract::AbstractTensor>(type_ptr, shape_vector);
  auto new_tensor_info = std::make_shared<tensor::Tensor>(type_ptr->type_id(), shape_vector);
  if (parameter->has_default()) {
    auto old_tensor_info = std::dynamic_pointer_cast<tensor::Tensor>(parameter->default_param());
    new_tensor_info = lite::CreateTensorInfo(old_tensor_info->data_c(), old_tensor_info->Size(),
                                             old_tensor_info->shape(), old_tensor_info->data_type());
    if (new_tensor_info == nullptr) {
      MS_LOG(ERROR) << "create tensor info failed.";
      return RET_ERROR;
    }
  }
  new_abstract->set_value(new_tensor_info);
  parameter->set_abstract(new_abstract);
  return RET_OK;
}

void InferShapePass::FreeTensors(std::vector<lite::Tensor *> *tensors) {
  for (auto tensor : *tensors) {
    delete tensor;
  }
  tensors->clear();
  tensors->shrink_to_fit();
}

STATUS InferShapePass::GetCNodeInputTensors(const CNodePtr &cnode, std::vector<lite::Tensor *> *input_tensors) {
  MS_ASSERT(cnode != nullptr);
  MS_ASSERT(input_tensors != nullptr);
  auto primitive = GetValueNode<PrimitivePtr>(cnode->input(0));
  if (primitive == nullptr) {
    MS_LOG(ERROR) << "primitive is nullptr: " << cnode->fullname_with_scope();
    return RET_ERROR;
  }
  const int WEIGHT_INDEX = 2;
  auto inputs = cnode->inputs();
  for (size_t i = 1; i < inputs.size(); ++i) {
    auto input = inputs[i];
    if (input == nullptr) {
      MS_LOG(ERROR) << "input is nullptr";
      return RET_ERROR;
    }

    if (utils::isa<ValueNodePtr>(cnode->input(i))) {
      MS_LOG(DEBUG) << cnode->fullname_with_scope() << "'s input[" << i << "] is value node";
      continue;
    }

    tensor::TensorPtr tensor_info;
    auto status = GetTensorInfoFromAbstract(&tensor_info, cnode, i);
    if (status != RET_OK) {
      MS_LOG(DEBUG) << "get tensor info failed.";
      return RET_ERROR;
    }
    std::unique_ptr<lite::Tensor> tensor = nullptr;
    if (tensor_info->data_type() != kObjectTypeTensorType) {
      tensor = std::make_unique<lite::Tensor>();
    } else {
      tensor = std::make_unique<lite::TensorList>();
    }
    if (tensor == nullptr) {
      MS_LOG(ERROR) << "new input tensor failed";
      return RET_ERROR;
    }

    std::vector<int> shape;
    std::transform(tensor_info->shape().begin(), tensor_info->shape().end(), std::back_inserter(shape),
                   [](const int64_t &value) { return static_cast<int32_t>(value); });
    if (tensor_info->data_type() != kObjectTypeTensorType) {
      tensor->set_shape(shape);
      tensor->set_data_type(tensor_info->data_type());
      if (primitive->GetAttr(opt::kWeightFormat) != nullptr && i == WEIGHT_INDEX) {
        tensor->set_format(static_cast<schema::Format>(GetValue<int64_t>(primitive->GetAttr(opt::kWeightFormat))));
      } else {
        tensor->set_format(schema::Format::Format_NHWC);
      }
    }

    if (utils::isa<ParameterPtr>(input)) {
      auto parameter = input->cast<ParameterPtr>();
      if (parameter->has_default()) {
        auto default_tensor_info = std::dynamic_pointer_cast<tensor::Tensor>(parameter->default_param());
        if (tensor_info->data_type() != kObjectTypeTensorType) {
          auto ret = tensor->MallocData();
          if (ret != 0) {
            MS_LOG(ERROR) << "Malloc tensor data failed";
            return RET_ERROR;
          }
          ret =
            memcpy_s(tensor->MutableData(), tensor->Size(), default_tensor_info->data_c(), default_tensor_info->Size());
          if (tensor->Size() != 0 && ret != EOK) {
            MS_LOG(ERROR) << "memcpy error: " << ret;
            return RET_ERROR;
          }
        } else {
          int *data = reinterpret_cast<int *>(default_tensor_info->data_c());
          auto tensor_list = reinterpret_cast<lite::TensorList *>(tensor.get());
          if (tensor_list->Decode(data) != RET_OK) {
            return RET_ERROR;
          }
        }
      }
    }
    input_tensors->push_back(tensor.release());
  }
  return RET_OK;
}

STATUS InferShapePass::GetCNodeOutputTensors(const CNodePtr &cnode, std::vector<lite::Tensor *> *output_tensors) {
  MS_ASSERT(output_tensors != nullptr);
  auto abstract = cnode->abstract();
  if (abstract == nullptr) {
    MS_LOG(ERROR) << "node " << cnode->fullname_with_scope() << " abstract is nullptr";
    return RET_ERROR;
  }
  std::vector<TypeId> types;
  if (utils::isa<abstract::AbstractTuple>(abstract)) {
    auto abstract_tuple = abstract->cast<abstract::AbstractTuplePtr>();
    auto elements = abstract_tuple->elements();
    for (auto &element : elements) {
      if (!utils::isa<abstract::AbstractTensorPtr>(element)) {
        MS_LOG(ERROR) << "abstract is not AbstractTensor";
        return RET_ERROR;
      }
      auto abstract_tensor = utils::cast<abstract::AbstractTensorPtr>(element);
      auto type_ptr = abstract_tensor->element()->GetTypeTrack();
      types.push_back(type_ptr->type_id());
    }
  } else {
    if (!utils::isa<abstract::AbstractTensorPtr>(abstract)) {
      MS_LOG(ERROR) << "abstract is not AbstractTensor";
      return RET_ERROR;
    }
    auto abstract_tensor = utils::cast<abstract::AbstractTensorPtr>(abstract);
    auto type_ptr = abstract_tensor->element()->GetTypeTrack();
    types.push_back(type_ptr->type_id());
  }
  for (auto &type : types) {
    std::unique_ptr<lite::Tensor> output_tensor = nullptr;
    if (type == kObjectTypeTensorType) {
      output_tensor = std::make_unique<lite::TensorList>();
    } else {
      output_tensor = std::make_unique<lite::Tensor>();
    }
    if (output_tensor == nullptr) {
      MS_LOG(ERROR) << "new output tensor failed";
      return RET_ERROR;
    }
    output_tensors->push_back(output_tensor.release());
  }
  return RET_OK;
}

STATUS InferShapePass::SetCNodeAbstract(const std::vector<lite::Tensor *> &output_tensors,
                                        const std::shared_ptr<CNode> &cnode) {
  MS_ASSERT(cnode != nullptr);
  if (output_tensors.size() == 0) {
    MS_LOG(ERROR) << "empty output_tensors";
    return RET_ERROR;
  }
  if (output_tensors.size() == 1) {
    auto tensor = output_tensors.front();
    auto new_abstract = ConvertLiteTensorToAbstractTensor(tensor);
    if (new_abstract == nullptr) {
      return RET_ERROR;
    }
    cnode->set_abstract(new_abstract);
  } else {
    AbstractBasePtrList abstract_list;
    for (size_t i = 0; i < output_tensors.size(); i++) {
      auto tensor = output_tensors.front();
      auto new_abstract = ConvertLiteTensorToAbstractTensor(tensor);
      if (new_abstract == nullptr) {
        return RET_ERROR;
      }
      abstract_list.emplace_back(new_abstract);
    }
    cnode->set_abstract(std::make_shared<abstract::AbstractTuple>(abstract_list));
  }
  return RET_OK;
}

int InferShapePass::StrIsContain(const std::vector<std::string> &total, const std::string &aim) {
  for (size_t i = 0; i < total.size(); i++) {
    if (aim.find(total[i]) != std::string::npos) {
      return i;
    }
  }
  return -1;
}

STATUS InferShapePass::SetSubGraphInputsAbstract(const CNodePtr &cnode, const FuncGraphPtr &func_graph) {
  // hard code construct input parameter name
  std::vector<std::string> inputs_names{};
  for (size_t i = 1; i < cnode->inputs().size(); i++) {
    inputs_names.emplace_back("_input_" + std::to_string(i - 1) + "_parameter");
  }
  // copy cnode input to func_graph input
  auto node_list = TopoSort(func_graph->get_return());
  for (auto &node : node_list) {
    if (utils::isa<ParameterPtr>(node)) {
      auto pos = StrIsContain(inputs_names, node->fullname_with_scope());
      if (pos != -1) {
        auto pnode = utils::cast<ParameterPtr>(node);
        auto input_pnode = utils::cast<ParameterPtr>(cnode->input(pos + 1));
        MS_ASSERT(pnode != nullptr);
        pnode->set_abstract(input_pnode->abstract());
      }
    }
  }
  return RET_OK;
}

bool InferShapePass::Run(const FuncGraphPtr &func_graph) {
  if (fmk_type != lite::converter::FmkType_TF && fmk_type != lite::converter::FmkType_TFLITE) {
    MS_LOG(INFO) << "The framework type of model should be tf/tflite.";
    return false;
  }
  MS_ASSERT(func_graph != nullptr);
  auto manager = func_graph->manager();
  MS_ASSERT(manager != nullptr);
  auto node_list = TopoSort(func_graph->get_return());
  for (auto &node : node_list) {
    if (utils::isa<ParameterPtr>(node)) {
      int status = SetParameterAbstract(node->cast<ParameterPtr>());
      if (status != RET_OK) {
        return false;
      }
      continue;
    }
    if (!utils::isa<CNodePtr>(node)) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    auto origin_primc = GetValueNode<PrimitiveCPtr>(cnode->input(0));
    if (origin_primc == nullptr) {
      auto sub_func_graph = GetValueNode<FuncGraphPtr>(cnode->input(0));
      if (sub_func_graph == nullptr) {
        MS_LOG(ERROR) << "node " << node->fullname_with_scope() << "'s origin_primc is nullptr";
        return false;
      } else {
        MS_LOG(WARNING) << "subgraph infer shape invalid.";
        return lite::RET_INFER_INVALID;
      }
    }
    if (IsSpecialType(cnode)) {
      continue;
    }
    std::vector<lite::Tensor *> input_tensors;
    std::vector<lite::Tensor *> output_tensors;
    auto status = GetCNodeInputTensors(cnode, &input_tensors);
    if (status != RET_OK) {
      MS_LOG(DEBUG) << "input shape unknown, infershape can't process cnode " << cnode->fullname_with_scope();
      FreeTensors(&input_tensors);
      continue;
    }
    status = GetCNodeOutputTensors(cnode, &output_tensors);
    if (status != RET_OK) {
      FreeTensors(&input_tensors);
      FreeTensors(&output_tensors);
      continue;
    }
    auto prim_t = lite::GetPrimitiveT(cnode->input(0));
    if (prim_t == nullptr) {
      MS_LOG(DEBUG) << "prim_t is nullptr";
      FreeTensors(&input_tensors);
      FreeTensors(&output_tensors);
      return false;
    }

    flatbuffers::FlatBufferBuilder fbb(INITIAL_SIZE);
    auto prim = lite::ConvertToPrimitive(prim_t.release(), &fbb);
    prim_t.reset();
    if (prim == nullptr) {
      MS_LOG(ERROR) << "get primitive failed.";
      FreeTensors(&input_tensors);
      FreeTensors(&output_tensors);
      fbb.Clear();
      return false;
    }
    auto parameter_gen =
      lite::PopulateRegistry::GetInstance()->GetParameterCreator(prim->value_type(), lite::SCHEMA_CUR);
    if (parameter_gen == nullptr) {
      MS_LOG(ERROR) << "PopulateParameter return nullptr, type: " << schema::EnumNamePrimitiveType(prim->value_type());
      FreeTensors(&input_tensors);
      FreeTensors(&output_tensors);
      fbb.Clear();
      return false;
    }
    auto parameter = parameter_gen(prim);
    if (parameter == nullptr) {
      MS_LOG(ERROR) << "parameter is nullptr.";
      FreeTensors(&input_tensors);
      FreeTensors(&output_tensors);
      fbb.Clear();
      return false;
    }
    parameter->infer_flag_ = true;
    status = KernelInferShape(input_tensors, &output_tensors, parameter);
    if (status == RET_OK) {
      status = SetCNodeAbstract(output_tensors, cnode);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "set CNode abstract failed: " << cnode->fullname_with_scope();
      }
    }
    FreeTensors(&input_tensors);
    FreeTensors(&output_tensors);
    free(parameter);
    fbb.Clear();
  }
  return true;
}
}  // namespace mindspore::opt
