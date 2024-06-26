/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "clientkit/nugu_runner.hh"
#include "base/nugu_log.h"

#include "core/nugu_runner_impl.hh"

namespace NuguClientKit {

struct NuguRunnerPrivate {
    NuguCore::NuguRunnerImpl impl;
};

NuguRunner::NuguRunner()
    : priv(std::unique_ptr<NuguRunnerPrivate>(new NuguRunnerPrivate()))
{
}

NuguRunner::~NuguRunner()
{
}

bool NuguRunner::invokeMethod(const std::string& tag, request_method method, ExecuteType type, int timeout)
{
    return priv->impl.invokeMethod(tag, std::move(method), type, timeout);
}

} // NuguClientKit
