/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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
#include "src/ops/populate/populate_register.h"
using mindspore::schema::PrimitiveType_PartialFusion;

namespace mindspore {
namespace lite {
typedef struct PartialParameter {
  OpParameter op_parameter_;
  int sub_graph_index_;
} PartialParameter;

OpParameter *PopulatePartialParameter(const void *prim) {
  auto *partial_parameter = reinterpret_cast<PartialParameter *>(malloc(sizeof(PartialParameter)));
  if (partial_parameter == nullptr) {
    MS_LOG(ERROR) << "malloc partial parameter failed.";
    return nullptr;
  }
  memset(partial_parameter, 0, sizeof(PartialParameter));
  auto primitive = static_cast<const schema::Primitive *>(prim);
  MS_ASSERT(primitive != nullptr);
  auto value = primitive->value_as_PartialFusion();
  if (value == nullptr) {
    MS_LOG(ERROR) << "value is nullptr";
    return nullptr;
  }
  partial_parameter->op_parameter_.type_ = primitive->value_type();
  partial_parameter->sub_graph_index_ = value->sub_graph_index();

  return reinterpret_cast<OpParameter *>(partial_parameter);
}
REG_POPULATE(PrimitiveType_PartialFusion, PopulatePartialParameter, SCHEMA_CUR)
}  // namespace lite
}  // namespace mindspore
