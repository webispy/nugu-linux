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

#include "capability/display_interface.hh"

namespace NuguCapability {

void IDisplayHandler::displayRendered(const std::string& id, const DisplayContextInfo& context_info) {}
void IDisplayHandler::displayCleared(const std::string& id) {}
void IDisplayHandler::elementSelected(const std::string& id, const std::string& item_token, const std::string& postback) {}
void IDisplayHandler::triggerChild(const std::string& ps_id, const std::string& data) {}
void IDisplayHandler::controlTemplate(const std::string& id, TemplateControlType control_type) {}
void IDisplayHandler::informControlResult(const std::string& id, ControlType type, ControlDirection direction, bool result) {}
void IDisplayHandler::setDisplayListener(IDisplayListener* listener) {}
void IDisplayHandler::removeDisplayListener(IDisplayListener* listener) {}
void IDisplayHandler::stopRenderingTimer(const std::string& id) {}
void IDisplayHandler::refreshRenderingTimer(const std::string& id) {}

} // NuguCapability
