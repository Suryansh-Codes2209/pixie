#!/bin/bash
# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

set -e

function finish {
  retval=$?
  /bin/exit_qemu_with_status "${retval}"
}

trap finish EXIT
trap finish ERR

function check_env_set() {
  if [[ -z "${!1}" ]]; then
    echo "The environment variable \"$1\" needs to be set"
    exit 1
  fi
}

# This file is generated by the test launcher.
# shellcheck disable=SC1091
source /test_fs/ssh_env.sh

check_env_set SSH_PUB_KEY

mkdir -p "/etc/ssh"
echo "PermitRootLogin without-password" >> /etc/ssh/sshd_config
mkdir -p "/root/.ssh"
cat "${SSH_PUB_KEY}" >> /root/.ssh/authorized_keys
ssh-keygen -A -N '' -b 1024 &> /dev/null
useradd sshd
mkdir -p "/run/sshd"
mkdir -p "/var/log"

echo "Starting SSH Daemon"
exec /usr/sbin/sshd -p 22 -e -D
