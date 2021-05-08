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

import ast
import argparse
import subprocess

parser = argparse.ArgumentParser(description="Run test_mobile_lenet.py case")
parser.add_argument("--device_target", type=str, default="CPU")
parser.add_argument("--server_mode", type=str, default="FEDERATED_LEARNING")
parser.add_argument("--worker_num", type=int, default=0)
parser.add_argument("--server_num", type=int, default=2)
parser.add_argument("--scheduler_ip", type=str, default="127.0.0.1")
parser.add_argument("--scheduler_port", type=int, default=8113)
parser.add_argument("--fl_server_port", type=int, default=6666)
parser.add_argument("--start_fl_job_threshold", type=int, default=1)
parser.add_argument("--fl_name", type=str, default="Lenet")
parser.add_argument("--fl_iteration_num", type=int, default=25)
parser.add_argument("--client_epoch_num", type=int, default=20)
parser.add_argument("--client_batch_size", type=int, default=32)
parser.add_argument("--secure_aggregation", type=ast.literal_eval, default=False)
parser.add_argument("--local_server_num", type=int, default=-1)

if __name__ == "__main__":
    args, _ = parser.parse_known_args()
    device_target = args.device_target
    server_mode = args.server_mode
    worker_num = args.worker_num
    server_num = args.server_num
    scheduler_ip = args.scheduler_ip
    scheduler_port = args.scheduler_port
    fl_server_port = args.fl_server_port
    start_fl_job_threshold = args.start_fl_job_threshold
    fl_name = args.fl_name
    fl_iteration_num = args.fl_iteration_num
    client_epoch_num = args.client_epoch_num
    client_batch_size = args.client_batch_size
    secure_aggregation = args.secure_aggregation
    local_server_num = args.local_server_num

    if local_server_num == -1:
        local_server_num = server_num

    assert local_server_num <= server_num, "The local server number should not be bigger than total server number."

    for i in range(local_server_num):
        cmd_server = "execute_path=$(pwd) && self_path=$(dirname \"${script_self}\") && "
        cmd_server += "rm -rf ${execute_path}/server_" + str(i) + "/ &&"
        cmd_server += "mkdir ${execute_path}/server_" + str(i) + "/ &&"
        cmd_server += "cd ${execute_path}/server_" + str(i) + "/ || exit && export GLOG_v=1 &&"
        cmd_server += "python ${self_path}/../test_mobile_lenet.py"
        cmd_server += " --device_target=" + device_target
        cmd_server += " --server_mode=" + server_mode
        cmd_server += " --ms_role=MS_SERVER"
        cmd_server += " --worker_num=" + str(worker_num)
        cmd_server += " --server_num=" + str(server_num)
        cmd_server += " --scheduler_ip=" + scheduler_ip
        cmd_server += " --scheduler_port=" + str(scheduler_port)
        cmd_server += " --fl_server_port=" + str(fl_server_port + i)
        cmd_server += " --start_fl_job_threshold=" + str(start_fl_job_threshold)
        cmd_server += " --fl_name=" + fl_name
        cmd_server += " --fl_iteration_num=" + str(fl_iteration_num)
        cmd_server += " --client_epoch_num=" + str(client_epoch_num)
        cmd_server += " --client_batch_size=" + str(client_batch_size)
        cmd_server += " --secure_aggregation=" + str(secure_aggregation)
        cmd_server += " > server.log 2>&1 &"

        import time
        time.sleep(0.3)
        subprocess.call(['bash', '-c', cmd_server])
