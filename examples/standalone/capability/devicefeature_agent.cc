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

#include <algorithm>
#include <iostream>

#include "devicefeature_agent.hh"

static const char* CAPABILITY_NAME = "DeviceFeature";
static const char* CAPABILITY_VERSION = "1.2";

DeviceFeatureAgent::DeviceFeatureAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void DeviceFeatureAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        devicefeature_listener = dynamic_cast<IDeviceFeatureListener*>(clistener);
}

void DeviceFeatureAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value features;
    Json::Value supported;

    supported.append("NAME");

    features["version"] = getVersion();
    features["supportedFeatures"] = supported;
    features["name"]["pocId"] = "speaker.nugu.nu110";
    features["name"]["deviceUniqueId"] = "NU110_FAKE00";
    features["versions"]["deviceApplicationVersion"] = "3.0.69";
    features["versions"]["deviceVersionNumber"] = "18820429";

    ctx[getName()] = features;
}
