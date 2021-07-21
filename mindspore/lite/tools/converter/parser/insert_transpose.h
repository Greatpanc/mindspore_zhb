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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_INSERT_TRANSPOSE_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_INSERT_TRANSPOSE_H_

#include <vector>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include "utils/utils.h"
#include "tools/converter/converter_flags.h"
#include "tools/optimizer/common/format_utils.h"
#include "tools/anf_exporter/fetch_content.h"

using mindspore::lite::converter::FmkType;
namespace mindspore {
namespace lite {
class InsertTranspose {
 public:
  InsertTranspose(FmkType fmk_type, bool train_flag) : fmk_type_(fmk_type), train_flag_(train_flag) {}
  ~InsertTranspose() = default;
  bool Run(const FuncGraphPtr &func_graph);
  STATUS InsertPostTransNode(const FuncGraphPtr &func_graph, const CNodePtr &cnode, const std::vector<int> &perm);
  STATUS InsertPreTransNode(const FuncGraphPtr &func_graph, const CNodePtr &cnode, const std::vector<int> &perm);
  STATUS GenNewInput(const FuncGraphPtr &func_graph, const CNodePtr &cnode, std::vector<int> perm, bool before,
                     size_t index = 0);

 private:
  AnfNodePtr GenNewInputWithoutShape(const FuncGraphPtr &func_graph, const CNodePtr &cnode,
                                     const std::vector<int> &perm, bool before, size_t index);
  bool ResetFuncGraph(const FuncGraphPtr &func_graph);
  bool BasicProcess(const FuncGraphPtr &func_graph, bool main_graph);
  void GetTransNodeFormatType(const CNodePtr &cnode, opt::TransTypePair *trans_info);
  STATUS HandleGraphInput(const FuncGraphPtr &func_graph, const CNodePtr &cnode);
  STATUS HandleGraphNode(const FuncGraphPtr &func_graph, const CNodePtr &cnode);
  void SetSubGraphInput(const CNodePtr &cnode, const FuncGraphPtr &sub_graph);
  void ResetSubGraphInput();
  void SetSubGraphOutput(const CNodePtr &cnode, const FuncGraphPtr &sub_graph);
  void SetSubGraphAbstract(const CNodePtr &cnode, const FuncGraphPtr &sub_graph);
  FmkType fmk_type_{lite::converter::FmkType_MS};
  bool train_flag_{false};
  std::unordered_map<FuncGraphPtr, std::vector<AnfNodePtr>> sub_inputs_map_;
};
}  // namespace lite
}  // namespace mindspore

#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_INSERT_TRANSPOSE_H_
