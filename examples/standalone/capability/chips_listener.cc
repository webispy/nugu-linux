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

#include <iostream>

#include "chips_listener.hh"

ChipsListener::ChipsListener()
{
}

void ChipsListener::onReceiveRender(const ChipsInfo& chips_info)
{
    if (!chips_info.contents.empty())
        std::cout << "[Chips] target:" << TARGET_TEXTS.at(chips_info.target) << std::endl;

    for (const auto& content : chips_info.contents)
        std::cout << " - text:" << content.text << std::endl;
}
