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

#ifdef NUGU_PLUGIN_BUILTIN_CBINDINGS
#define NUGU_PLUGIN_BUILTIN
#endif

#include <glib.h>

#include <capability/audio_player_interface.hh>
#include <capability/capability_factory.hh>
#include <capability/system_interface.hh>
#include <capability/text_interface.hh>
#include <capability/tts_interface.hh>
#include <clientkit/nugu_client.hh>

#include <clientkit/speech_recognizer_aggregator_interface.hh>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"

using namespace NuguClientKit;
using namespace NuguCapability;

extern "C" {
// NUGU C Bindings
typedef void (*ncb_type_network_callback)(NetworkStatus status);
typedef void (*ncb_type_network_error_callback)(NetworkError error_code);

EXPORT_API int ncb_init();
EXPORT_API int ncb_deinit();
EXPORT_API int ncb_start();
EXPORT_API int ncb_stop();
EXPORT_API int ncb_set_token(const char *token);
EXPORT_API int ncb_connect();
EXPORT_API int ncb_text_send(const char *text);
EXPORT_API int ncb_set_network_callback(ncb_type_network_callback callback);
EXPORT_API int
ncb_set_network_error_callback(ncb_type_network_error_callback callback);
EXPORT_API int ncb_start_listening_with_wakeup();
EXPORT_API int ncb_stop_listening();
EXPORT_API int ncb_start_listening();
}

static ncb_type_network_callback _network_callback;
static ncb_type_network_error_callback _network_error_callback;

class MyTTSListener : public ITTSListener {
    public:
	virtual ~MyTTSListener() = default;

	void onTTSState(TTSState state, const std::string &dialog_id) override
	{
	}

	void onTTSText(const std::string &text,
		       const std::string &dialog_id) override
	{
		std::cout << "TTS: " << text << std::endl;
	}

	void onTTSCancel(const std::string &dialog_id) override
	{
	}
};

class MySRAListener : public ISpeechRecognizerAggregatorListener {
    public:
	virtual ~MySRAListener() = default;

	void onWakeupState(WakeupDetectState state, float power_noise,
			   float power_speech) override
	{
	}
	void onASRState(ASRState state, const std::string &dialog_id,
			ASRInitiator initiator) override
	{
	}
	void onResult(const RecognitionResult &result,
		      const std::string &dialog_id) override
	{
	}
};

class MyNetwork : public INetworkManagerListener {
    public:
	void onStatusChanged(NetworkStatus status) override
	{
		switch (status) {
		case NetworkStatus::DISCONNECTED:
			std::cout << "Network disconnected !" << std::endl;
			break;
		case NetworkStatus::CONNECTING:
			std::cout << "Network connecting..." << std::endl;
			break;
		case NetworkStatus::READY:
			std::cout << "Network ready !" << std::endl;
			break;
		case NetworkStatus::CONNECTED:
			std::cout << "Network connected !" << std::endl;
			break;
		default:
			break;
		}

		if (_network_callback)
			_network_callback(status);
	}

	void onError(NetworkError error) override
	{
		switch (error) {
		case NetworkError::FAILED:
			std::cout << "Network failed !" << std::endl;
			break;
		case NetworkError::TOKEN_ERROR:
			std::cout << "Token error !" << std::endl;
			break;
		case NetworkError::UNKNOWN:
			std::cout << "Unknown error !" << std::endl;
			break;
		}

		if (_network_error_callback)
			_network_error_callback(error);
	}
};

static MyTTSListener *tts_listener = nullptr;
static MyNetwork *network_listener = nullptr;
static MySRAListener *sra_listener = nullptr;

static std::shared_ptr<NuguClient> nugu_client;
static std::shared_ptr<ISystemHandler> system_handler = nullptr;
static std::shared_ptr<IAudioPlayerHandler> audio_player_handler = nullptr;
static std::shared_ptr<ITextHandler> text_handler = nullptr;
static std::shared_ptr<ITTSHandler> tts_handler = nullptr;
static std::shared_ptr<IASRHandler> asr_handler = nullptr;

int ncb_init()
{
	nugu_info("ncb_init");

	nugu_client = std::make_shared<NuguClient>();

	/* Create System, AudioPlayer capability default */
	system_handler = std::shared_ptr<ISystemHandler>(
		CapabilityFactory::makeCapability<SystemAgent,
						  ISystemHandler>());
	audio_player_handler = std::shared_ptr<IAudioPlayerHandler>(
		CapabilityFactory::makeCapability<AudioPlayerAgent,
						  IAudioPlayerHandler>());
	asr_handler = std::shared_ptr<IASRHandler>(
		CapabilityFactory::makeCapability<ASRAgent, IASRHandler>());
	asr_handler->setAttribute(ASRAttribute{
		"/opt/homebrew/share/nugu/model" });

	/* Create a Text capability */
	text_handler = std::shared_ptr<ITextHandler>(
		CapabilityFactory::makeCapability<TextAgent, ITextHandler>());

	/* Create a TTS capability */
	tts_listener = new MyTTSListener();
	tts_handler = std::shared_ptr<ITTSHandler>(
		CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(
			tts_listener));

	/* Register build-in capabilities */
	nugu_client->getCapabilityBuilder()
		->add(system_handler.get())
		->add(audio_player_handler.get())
		->add(tts_handler.get())
		->add(text_handler.get())
		->add(asr_handler.get())
		->setWakeupModel(
			{ "/opt/homebrew/share/nugu/model/skt_trigger_am_aria.raw",
			  "/opt/homebrew/share/nugu/model/skt_trigger_search_aria.raw" })
		->construct();

	if (!nugu_client->initialize()) {
		nugu_info("SDK Initialization failed.");
		return -1;
	}

	sra_listener = new MySRAListener();
	nugu_client->getSpeechRecognizerAggregator()->addListener(sra_listener);

	/* Network manager */
	network_listener = new MyNetwork();
	nugu_client->getNetworkManager()->addListener(network_listener);

	return 0;
}

int ncb_deinit()
{
	nugu_info("ncb_deinit");

	nugu_client->deInitialize();

	delete tts_listener;
	delete network_listener;
	delete sra_listener;

	return 0;
}

int ncb_set_token(const char *token)
{
	nugu_info("ncb_set_token");

	auto network_manager(nugu_client->getNetworkManager());
	network_manager->setToken(token);

	return 0;
}

int ncb_connect()
{
	nugu_info("ncb_connect");
	nugu_client->getNetworkManager()->connect();

	return 0;
}

int ncb_text_send(const char *text)
{
	g_return_val_if_fail(text != NULL, -1);

	nugu_info("send text: %s", text);
	text_handler->requestTextInput(text);

	return 0;
}

int ncb_set_network_callback(ncb_type_network_callback callback)
{
	_network_callback = callback;
	return 0;
}

int ncb_set_network_error_callback(ncb_type_network_error_callback callback)
{
	_network_error_callback = callback;
	return 0;
}

int ncb_start_listening_with_wakeup()
{
	nugu_client->getSpeechRecognizerAggregator()
		->startListeningWithTrigger();
	return 0;
}

int ncb_stop_listening()
{
	nugu_client->getSpeechRecognizerAggregator()->stopListening();
	return 0;
}

int ncb_start_listening()
{
	nugu_client->getSpeechRecognizerAggregator()->startListening();
	return 0;
}

static int init(NuguPlugin *p)
{
	nugu_dbg("plugin-init '%s'", nugu_plugin_get_description(p)->name);

	return 0;
}

static int load(void)
{
	nugu_dbg("plugin-load");

	return 0;
}

static void unload(NuguPlugin *p)
{
	nugu_dbg("plugin-unload '%s'", nugu_plugin_get_description(p)->name);
}

extern "C" {

NUGU_PLUGIN_DEFINE(cbindings, NUGU_PLUGIN_PRIORITY_DEFAULT, "0.0.1", load,
		   unload, init);
}
