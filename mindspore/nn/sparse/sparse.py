# Copyright 2020 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""Sparse related tools."""
from mindspore.ops import operations as P
from ..cell import Cell


class SparseToDense(Cell):
    """
    Convert a sparse tensor into dense.

    Not yet supported by any backend at the moment.

    Args:
        sparse_tensor (SparseTensor): the sparse tensor to convert.

    Returns:
        Tensor, the tensor converted.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> from mindspore import Tensor, SparseTensor
        >>> import mindspore.nn as nn
        >>> indices = Tensor([[0, 1], [1, 2]])
        >>> values = Tensor([1, 2], dtype=ms.int32)
        >>> dense_shape = (3, 4)
        >>> sparse_tensor = SparseTensor(indices, values, dense_shape)
        >>> sparse_to_dense = nn.SparseToDense()
        >>> result = sparse_to_dense(sparse_tensor)
        >>> print(result)
        [[0 1 0 0]
         [0 0 2 0]
         [0 0 0 0]]
    """
    def __init__(self):
        super(SparseToDense, self).__init__()
        self.sparse_to_dense = P.SparseToDense()

    def construct(self, sparse_tensor):
        return self.sparse_to_dense(sparse_tensor.indices,
                                    sparse_tensor.values,
                                    sparse_tensor.dense_shape)
