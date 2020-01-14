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

#include <string.h>

#include "base/nugu_log.h"

#include "capability_manager.hh"
#include "extension_agent.hh"

namespace NuguCore {

static const char* capability_version = "1.0";

ExtensionAgent::ExtensionAgent()
    : Capability(CapabilityType::Extension, capability_version)
    , extension_listener(nullptr)
    , ps_id("")
{
}

ExtensionAgent::~ExtensionAgent()
{
}

void ExtensionAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "Action"))
        parsingAction(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }
}

void ExtensionAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value extension;

    extension["version"] = getVersion();

    if (extension_listener) {
        std::string data;
        extension_listener->requestContext(data);

        if (data.size()) {
            Json::Value root;
            Json::Reader reader;

            if (reader.parse(data.c_str(), root))
                extension["data"] = data;
            else
                nugu_error("context data is not json format\n%s", data.c_str());
        }
    }

    ctx[getName()] = extension;
}

void ExtensionAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        extension_listener = dynamic_cast<IExtensionListener*>(clistener);
}

void ExtensionAgent::sendEventActionSucceeded()
{
    sendEventCommon("ActionSucceeded");
}

void ExtensionAgent::sendEventActionFailed()
{
    sendEventCommon("ActionFailed");
}

void ExtensionAgent::sendEventCommon(const std::string& ename)
{
    std::string payload = "";
    Json::Value root;
    Json::StyledWriter writer;

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void ExtensionAgent::parsingAction(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    Json::Value data = root["data"];

    if (data.isNull()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (extension_listener) {
        Json::StyledWriter writer;
        std::string action = writer.write(data);

        if (extension_listener->receiveAction(action) == ExtensionResult::SUCCEEDED)
            sendEventActionSucceeded();
        else
            sendEventActionFailed();
    }
}

} // NuguCore