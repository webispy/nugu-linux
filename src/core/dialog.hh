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

#ifndef __NUGU_DIALOG_H__
#define __NUGU_DIALOG_H__

#include <string>
#include <list>

namespace NuguCore {

class NuguAction {
public:
    NuguAction();
    ~NuguAction();

    void done();
};

class NuguDialog {
public:
    NuguDialog();
    ~NuguDialog();

    std::string dialog_id;

    std::list<NuguAction> actions;

    void onForeground();
    void onBackground();
};

}

#endif