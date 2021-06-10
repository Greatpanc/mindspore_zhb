#!/bin/bash
# Copyright 2021 Huawei Technologies Co., Ltd
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

if [ $# != 5 ]
then
    echo "============================================================================================================"
    echo "Please run the script as: "
    echo "sh scripts/run_train.sh [TASK_NAME] [DEVICE_TARGET] [TEACHER_MODEL_DIR] [STUDENT_MODEL_DIR] [DATA_DIR]"
    echo "============================================================================================================"
exit 1
fi

echo "===============================================start training==============================================="

task_name=$1
device_target=$2
teacher_model_dir=$3
student_model_dir=$4
data_dir=$5

mkdir -p ms_log
PROJECT_DIR=$(cd "$(dirname "$0")" || exit; pwd)
CUR_DIR=`pwd`
export GLOG_log_dir=${CUR_DIR}/ms_log
export GLOG_logtostderr=0
python ${PROJECT_DIR}/../train.py \
    --task_name=$task_name \
    --device_target=$device_target \
    --device_id=0 \
    --teacher_model_dir=$teacher_model_dir \
    --student_model_dir=$student_model_dir \
    --data_dir=$data_dir> log.txt 2>&1 &