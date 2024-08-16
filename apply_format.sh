#!/bin/bash
##
## Copyright (c) 2024 The Johns Hopkins University Applied Physics
## Laboratory LLC.
##
## This file is part of the BPSec Library (BSL).
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
## This work was performed for the Jet Propulsion Laboratory, California
## Institute of Technology, sponsored by the United States Government under
## the prime contract 80NM0018D0004 between the Caltech and NASA under
## subcontract 1700763.
##

#
# From a fresh checkout perform pre-build steps
#
set -e

SELFDIR=$(realpath $(dirname "${BASH_SOURCE[0]}"))
cd ${SELFDIR}

if [[ "$#" -ne 0 ]]
then
    ARGS="$@"
else
    ARGS="src/case/*.h src/case/*.c src/case/ari/*.h src/case/ari/*.c src/case/amm/*.h src/case/amm/*.c test/*.h test/*.c"
fi

clang-format --style=file -i $ARGS
