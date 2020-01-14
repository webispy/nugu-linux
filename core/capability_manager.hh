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

#ifndef __NUGU_CAPABILITY_AGENT_H__
#define __NUGU_CAPABILITY_AGENT_H__

#include <map>
#include <memory>

#include "base/nugu_directive_sequencer.h"
#include "base/nugu_focus.h"
#include "interface/capability/capability_interface.hh"

#include "playsync_manager.hh"

namespace NuguCore {

class IFocusListener {
public:
    virtual ~IFocusListener() = default;
    virtual NuguFocusResult onFocus(void* event) = 0;
    virtual NuguFocusResult onUnfocus(void* event) = 0;
    virtual NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) = 0;
};

class CapabilityManager {
private:
    CapabilityManager();
    virtual ~CapabilityManager();

public:
    static CapabilityManager* getInstance();
    static void destroyInstance();
    PlaySyncManager* getPlaySyncManager();

    static DirseqReturn dirseqCallback(NuguDirective* ndir, void* userdata);

    void addCapability(const std::string& cname, ICapabilityInterface* cap);
    void removeCapability(const std::string& cname);

    void setWakeupWord(const std::string& word);

    std::string makeContextInfo(Json::Value& ctx);
    std::string makeAllContextInfo();
    std::string makeAllContextInfoStack();

    void preprocessDirective(NuguDirective* ndir);
    bool isSupportDirectiveVersion(const std::string& version, ICapabilityInterface* cap);

    void sendCommand(CapabilityType from, CapabilityType to, const std::string& command, const std::string& param);
    void sendCommandAll(const std::string& command, const std::string& param);
    void getCapabilityProperty(CapabilityType cap, const std::string& property, std::string& value);
    void getCapabilityProperties(CapabilityType cap, const std::string& property, std::list<std::string>& values);

    bool isFocusOn();
    int addFocus(const std::string& fname, NuguFocusType type, IFocusListener* listener);
    int removeFocus(const std::string& fname);
    int requestFocus(const std::string& fname, void* event);
    int releaseFocus(const std::string& fname);

private:
    ICapabilityInterface* findCapability(const std::string& cname);

    static CapabilityManager* instance;
    std::map<std::string, ICapabilityInterface*> caps;
    std::map<std::string, NuguFocus*> focusmap;
    std::string wword;
    std::unique_ptr<PlaySyncManager> playsync_manager = nullptr;
};

} // NuguCore

#endif