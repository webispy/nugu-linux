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

#ifndef __NUGU_CHIPS_AGENT_H__
#define __NUGU_CHIPS_AGENT_H__

#include "capability/chips_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class ChipsAgent : public Capability,
                   public IChipsHandler {
public:
    ChipsAgent();
    virtual ~ChipsAgent() = default;

    void initialize() override;

    void parsingDirective(const char* dname, const char* message) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void addListener(IChipsListener* listener) override;
    void removeListener(IChipsListener* listener) override;
    void updateInfoForContext(NJson::Value& ctx) override;

private:
    void parsingRender(const char* message);

    const std::map<std::string, ChipsTarget> TARGETS {
        { "DM", ChipsTarget::DM },
        { "LISTEN", ChipsTarget::LISTEN },
        { "SPEAKING", ChipsTarget::SPEAKING }

    };
    const std::map<std::string, ChipsType> CHIPS_TYPES {
        { "NUDGE", ChipsType::NUDGE },
        { "ACTION", ChipsType::ACTION },
        { "GENERAL", ChipsType::GENERAL }

    };

    std::vector<IChipsListener*> chips_listeners;
};

} // NuguCapability

#endif /* __NUGU_CHIPS_AGENT_H__ */
