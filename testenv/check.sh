#!/bin/bash
##
## Copyright (c) 2023 The Johns Hopkins University Applied Physics
## Laboratory LLC.
##
## This file is part of the Delay-Tolerant Networking Management
## Architecture (DTNMA) Tools package.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##     http://www.apache.org/licenses/LICENSE-2.0
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
set -e

SELFDIR=$(realpath $(dirname "${BASH_SOURCE[0]}"))
cd "${SELFDIR}"

docker-compose ps

DEXEC="docker-compose exec -T nm-mgr"

# Wait a few seconds for ION to start
for IX in $(seq 10)
do
  sleep 1
  if ${DEXEC} service_is_running ion
  then
    break
  fi
  echo "Waiting for ion..."
done

for SVC in ion ion-nm-mgr ion-nm-agent
do
  echo
  if ! ${DEXEC} service_is_running ${SVC}
  then
    echo "Logs for ${SVC}:"
    ${DEXEC} journalctl --unit ${SVC}
  fi
done
