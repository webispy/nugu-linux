
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

#ifndef __NUGU_SDK_MANAGER_H__
#define __NUGU_SDK_MANAGER_H__

#include <memory>

#include <clientkit/nugu_client.hh>

#include "capability_collection.hh"
#include "nugu_sample_manager.hh"
#include "speaker_status.hh"

class SpeakerController {
public:
    using CapabilityHandler = struct {
        ITTSHandler* tts;
        IAudioPlayerHandler* audio_player;
        ISpeakerHandler* speaker;
    };

public:
    SpeakerController();
    virtual ~SpeakerController() = default;

    void setCapabilityHandler(CapabilityHandler&& handler);
    void setVolumeUp();
    void setVolumeDown();
    void toggleMute();

private:
    void composeVolumeControl();
    void composeMuteControl();
    void adjustVolume(int volume);

    std::function<bool(int)> nugu_speaker_volume = nullptr;
    std::function<bool(bool)> nugu_speaker_mute = nullptr;
    CapabilityHandler capability_handler {};
};

class NuguSDKManager : public IFocusManagerObserver,
                       public INetworkManagerListener,
                       public IInteractionControlManagerListener {
public:
    explicit NuguSDKManager(NuguSampleManager* nugu_sample_manager);
    virtual ~NuguSDKManager() = default;

    void setup();

    // implements IFocusManagerObserver
    void onFocusChanged(const FocusConfiguration& configuration, FocusState state, const std::string& name) override;

    // implements INetworkManagerListener
    void onStatusChanged(NetworkStatus status) override;
    void onError(NetworkError error) override;

    // implements IInteractionControlManagerListener
    void onHasMultiTurn() override;
    void onModeChanged(bool is_multi_turn) override;

private:
    void composeExecuteCommands();
    void composeSDKCommands();
    void createInstance();
    void registerCapabilities();
    void setDefaultSoundLayerPolicy();
    void setAdditionalExecutor();
    void deleteInstance();

    void initSDK();
    void deInitSDK(bool is_exit = false);
    void reset();
    void exit();

    const int TEXT_INPUT_TYPE_1 = 1;
    const int TEXT_INPUT_TYPE_2 = 2;

    std::shared_ptr<SpeakerController> speaker_controller = nullptr;
    std::shared_ptr<NuguClient> nugu_client = nullptr;
    std::shared_ptr<CapabilityCollection> capa_collection = nullptr;
    std::shared_ptr<IWakeupHandler> wakeup_handler = nullptr;
    NuguSampleManager* nugu_sample_manager = nullptr;
    INuguCoreContainer* nugu_core_container = nullptr;
    IPlaySyncManager* playsync_manager = nullptr;
    INetworkManager* network_manager = nullptr;
    SpeechOperator* speech_operator = nullptr;
    ITextHandler* text_handler = nullptr;
    IMicHandler* mic_handler = nullptr;
    SpeakerStatus* speaker_status = nullptr;

    std::function<void()> on_init_func = nullptr;
    std::function<void()> on_fail_func = nullptr;

    bool sdk_initialized = false;
    bool is_network_error = false;
};

#endif /* __NUGU_SDK_MANAGER_H__ */
