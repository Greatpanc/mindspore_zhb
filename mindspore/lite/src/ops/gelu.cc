/**
 * Copyright 2019-2020 Huawei Technologies Co., Ltd
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

#include "src/ops/gelu.h"
#include <memory>
#include "include/errorcode.h"
#include "src/common/log_adapter.h"
#include "src/tensor.h"
#ifndef PRIMITIVE_WRITEABLE
#include "src/ops/ops_register.h"
#endif

namespace mindspore {
namespace lite {
#ifdef PRIMITIVE_WRITEABLE
int GeLU::UnPackAttr(const Primitive &prim, const std::vector<AnfNodePtr> &inputs) {
  if (this->primitive_ == nullptr) {
    this->primitive_ = new (std::nothrow) schema::PrimitiveT;
    if (this->primitive_ == nullptr) {
      MS_LOG(ERROR) << "new primitiveT failed";
      return RET_ERROR;
    }
    this->primitive_->value.type = schema::PrimitiveType_GeLU;
  }
  if (this->primitive_->value.type != schema::PrimitiveType_GeLU) {
    MS_LOG(ERROR) << "Primitive type is error :" << this->primitive_->value.type;
    return RET_ERROR;
  }
  if (this->primitive_->value.value == nullptr) {
    this->primitive_->value.value = new (std::nothrow) schema::GeLUT();
    if (this->primitive_->value.value == nullptr) {
      MS_LOG(ERROR) << "new primitiveT value failed";
      return RET_ERROR;
    }
  }
  return RET_OK;
}
#else
int GeLU::UnPackToFlatBuilder(const schema::Primitive *primitive, flatbuffers::FlatBufferBuilder *fbb) {
  MS_ASSERT(nullptr != primitive);
  MS_ASSERT(nullptr != fbb);
  auto attr = primitive->value_as_GeLU();
  if (attr == nullptr) {
    MS_LOG(ERROR) << "value_as_Gather return nullptr";
    return RET_ERROR;
  }

  auto val_offset = schema::CreateGeLU(*fbb);
  auto prim_offset = schema::CreatePrimitive(*fbb, schema::PrimitiveType_GeLU, val_offset.o);
  fbb->Finish(prim_offset);
  return RET_OK;
}

PrimitiveC *GeLUCreator(const schema::Primitive *primitive) { return PrimitiveC::NewPrimitiveC<GeLU>(primitive); }
Registry GeLURegistry(schema::PrimitiveType_GeLU, GeLUCreator);
#endif
}  // namespace lite
}  // namespace mindspore
