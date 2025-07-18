#include <Preferences.h>
#include <chrono>
#include <vector>
#include <map>
#include <pb_decode.h>

#include "configuration.h"
#include "Channels.h"
#include "CrisislabCommon.h"
#include "Router.h"
#include "MeshService.h"
#include "GatewayModule.h"
#include "NormalNodeModule.h"
#include "mqtt/MQTT.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "../Telemetry/DeviceTelemetry.h"

std::unique_ptr<std::map<uint32_t, meshtastic_CrisislabMessage_NextHops>> CrisislabCommon::nextHopsMapPtr(
    new std::map<uint32_t, meshtastic_CrisislabMessage_NextHops>()
);

bool meshtastic_CrisislabMessage_NextHopsMap_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field) {
	if (ostream) {
		LOG_ERROR("CrisislabMessage_NextHopsMap callback called with ostream, but this is not supported");
		return false;
	}

	if (!istream) {
		LOG_ERROR("CrisislabMessage_NextHopsMap_callback callback called with null istream");
		return false;
	}

	meshtastic_CrisislabMessage_NextHopsMap_EntriesEntry entry;

	if (istream->bytes_left) {
		if (!pb_decode(istream, meshtastic_CrisislabMessage_NextHopsMap_EntriesEntry_fields, &entry)) {
			LOG_ERROR("Failed to decode next hops entry entry: %s", PB_GET_ERROR(istream));
			return false;
		}

		CrisislabCommon::nextHopsMapPtr->insert(std::make_pair(entry.key, entry.value));
	} else {
		LOG_DEBUG("No bytes left in stream (CrisislabMessage_NextHopsMap callback)");
	}

	return true;
}

uint64_t CrisislabCommon::secondsSinceEpoch() {
#if MESHTASTIC_CRISISLAB_GATEWAY
	const auto sinceEpoch = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(sinceEpoch).count();
#else
	return 0;
#endif
}

bool CrisislabCommon::decodeCrisislabMessageFromBytes(
	byte *buffer,
	unsigned int bufferLength,
	meshtastic_CrisislabMessage *msg
) {
	pb_istream_t stream = pb_istream_from_buffer(buffer, bufferLength);

	if (!pb_decode(&stream, meshtastic_CrisislabMessage_fields, msg)) {
		LOG_ERROR("Failed to decode CrisislabMessage from bytes: %s", PB_GET_ERROR(&stream));
		return false;
	}

	LOG_DEBUG("Decoded CrisislabMessage from bytes, which_message = %d", msg->which_message);

	return true;
}

CrisislabCommon::CrisislabCommon(const char *name) :
	MeshModule(name)
{
	Preferences preferences;
	preferences.begin("crisislab", true);

	// log preferences

	const std::string prefNames[] = {
		"bcast_interval",
		"channel_name",
		"ping_timeout"
	};

	for (const auto &pref : prefNames) {
		if (!preferences.isKey(pref.c_str())) {
			LOG_ERROR("Crisislab setting %s not found", pref.c_str());
			continue;
		}

		switch (preferences.getType(pref.c_str())) {
			case PT_U32:
				LOG_INFO("Crisislab setting: %s = %u", pref.c_str(),
			 		preferences.getUInt(pref.c_str()));
				break;
			case PT_STR:
				LOG_INFO("Crisislab setting: %s = %s", pref.c_str(),
			 		preferences.getString(pref.c_str()).c_str());
				break;
			default:
				LOG_WARN("Logging crisislab setting %s not implemented");
		}
	}

	// load channel index into memory
	if (preferences.isKey("channel_name")) {
		char channelName[12];
		preferences.getString("channel_name", channelName, 12);
		CrisislabCommon::channelIndex = channels.getByName(channelName).index;
	}

	preferences.end();
}

meshtastic_MeshPacket *CrisislabCommon::allocMeshPacket(NodeNum to, meshtastic_PortNum portNum)
{
	meshtastic_MeshPacket *meshPacket = router->allocForSending();

	meshPacket->to = to;
	meshPacket->from = nodeDB->getNodeNum();
	meshPacket->channel = this->channelIndex;
	meshPacket->priority = meshtastic_MeshPacket_Priority_RELIABLE;
	meshPacket->decoded.portnum = portNum;

	LOG_DEBUG("allocating meshpacket to 0x%x, from 0x%x, channel %d",
		meshPacket->to,
		meshPacket->from,
		meshPacket->channel
	);

	return meshPacket;
}

void CrisislabCommon::handleCrisislabMessage(
	const meshtastic_CrisislabMessage &message,
	const meshtastic_MeshPacket *meshPacket
) {
	switch (message.which_message) {
		case meshtastic_CrisislabMessage_mesh_settings_tag: {
			LOG_INFO("Handling \"mesh settings\" command");

			Preferences preferences;
			preferences.begin("crisislab", false);

			meshtastic_CrisislabMessage_MeshSettings meshSettings = message.message.mesh_settings;

			if (meshSettings.has_channel_name) {
				LOG_INFO("Setting channel name to \"%s\"", meshSettings.channel_name);
				preferences.putString("channel_name", meshSettings.channel_name);
			}

			if (meshSettings.has_broadcast_interval_seconds) {
				LOG_INFO("Setting broadcast interval to %u seconds", meshSettings.broadcast_interval_seconds);
				preferences.putUInt("bcast_interval", meshSettings.broadcast_interval_seconds);
			}

			if (meshSettings.has_ping_timeout_seconds) {
				LOG_INFO("Setting ping timeout to %u seconds", meshSettings.ping_timeout_seconds);
				preferences.putUInt("ping_timeout", meshSettings.ping_timeout_seconds);
			}

			preferences.end();

			break;
		}
		case meshtastic_CrisislabMessage_get_mesh_settings_request_tag: {
#if MESHTASTIC_CRISISLAB_GATEWAY
			LOG_INFO("Handling \"get mesh settings request\" command");

			Preferences preferences;
			preferences.begin("crisislab", true);

			meshtastic_CrisislabMessage responseMessage = meshtastic_CrisislabMessage_init_default;
			responseMessage.which_message = meshtastic_CrisislabMessage_mesh_settings_tag;

			meshtastic_CrisislabMessage_MeshSettings *settings = &responseMessage.message.mesh_settings;

			settings->has_channel_name = preferences.isKey("channel_name");
			if (settings->has_channel_name) {
				preferences.getString("channel_name", settings->channel_name, sizeof(settings->channel_name));
			}

			settings->has_broadcast_interval_seconds = preferences.isKey("bcast_interval");
			if (settings->has_broadcast_interval_seconds) {
				settings->broadcast_interval_seconds = preferences.getUInt("bcast_interval");
			}

			settings->has_ping_timeout_seconds = preferences.isKey("ping_timeout");
			if (settings->has_ping_timeout_seconds) {
				settings->ping_timeout_seconds = preferences.getUInt("ping_timeout");
			}

			preferences.end();

			uint8_t encodedResponseMessage[MESHTASTIC_MESHTASTIC_CRISISLAB_PB_H_MAX_SIZE];

			size_t bytesWritten = pb_encode_to_bytes(
				encodedResponseMessage,
				sizeof(encodedResponseMessage),
				&meshtastic_CrisislabMessage_msg,
				&responseMessage
			);

			GatewayModule::tryMqttPublish(
				encodedResponseMessage,
				bytesWritten
			);

			LOG_DEBUG("Sent mesh settings response to MQTT");
#else
			LOG_INFO("Ignoring \"get mesh settings request\" command, we are not a gateway");
#endif

			break;
		}
		case meshtastic_CrisislabMessage_update_next_hops_request_tag: {
			LOG_INFO("Handling \"update next hops request\" command");

			if (this->ignoreUpdateNextHopsRequestPackets) {
				LOG_DEBUG("Ignoring update next hops packet");
				break;
			}

			this->ignoreUpdateNextHopsRequestPackets = true;

			// create ping message to be broadcast

			meshtastic_CrisislabMessage pingMessage = meshtastic_CrisislabMessage_init_default;

			pingMessage.which_message = meshtastic_CrisislabMessage_ping_tag;

			// wrap in packet for the mesh

			meshtastic_MeshPacket *pingPacket = this->allocMeshPacket();
			pingPacket->hop_limit = 1;
			pingPacket->decoded.payload.size = pb_encode_to_bytes(
				pingPacket->decoded.payload.bytes,
				sizeof(pingPacket->decoded.payload.bytes),
				&meshtastic_CrisislabMessage_msg,
				&pingMessage
			);

			service->sendToMesh(pingPacket);

			LOG_DEBUG("Sent ping for signal data collection");

			break;
		}
		case meshtastic_CrisislabMessage_ping_tag: {
			LOG_INFO("Handling \"ping\" command from node %u", meshPacket->from);

			if (meshPacket == nullptr) {
				LOG_ERROR("Received ping timestamp without mesh packet");
				break;
			}

			if (!this->isCollectingPings) {
				this->isCollectingPings = true;
				this->signalDataEntries.clear();
				xTaskCreate(
					CrisislabCommon::returnSignalData,
					"PING_COLLECTION_TIMEOUT",
					4096, // stack size in words
					this, // task parameter
					5, // priority from 0-9
					NULL // task handle
				);
			}

			this->signalDataEntries.push_back({
				.from = meshPacket->from,
				.rssi = meshPacket->rx_rssi,
				.snr = meshPacket->rx_snr
			});

			LOG_DEBUG("Wrote signal data for node %u: RSSI: %d, SNR: %f",
				meshPacket->from,
				meshPacket->rx_rssi,
				meshPacket->rx_snr
			);

			break;
		}
		case meshtastic_CrisislabMessage_signal_data_tag: {
			LOG_INFO("Handling \"signal data\" command from node %u", meshPacket->from);

			// this packet should be ignored by all normal nodes
			// NOTE: later we may need to rebroadcast if some signal data packets aren't making it

			if (mqtt == nullptr) {
				LOG_DEBUG("MQTT not initialized, ignoring signal data");
				break;
			}

			GatewayModule::tryMqttPublish(
				meshPacket->decoded.payload.bytes,
				meshPacket->decoded.payload.size
			);

			break;
		}
		case meshtastic_CrisislabMessage_updated_next_hops_tag: {
			LOG_INFO("Handling \"updated next hops\" command");

			this->ignoreUpdateNextHopsRequestPackets = false;

#if MESHTASTIC_CRISISLAB_GATEWAY
			LOG_DEBUG("We are a gateway, no need to handle updated next hops");
			break;
#endif

			auto *nextHopsMap = CrisislabCommon::nextHopsMapPtr.get();

			if (nextHopsMap == nullptr) {
				LOG_ERROR("handleCrisislabMessage received updated next hops with null map");
				break;
			}

			LOG_DEBUG("Got next hops map object from crisislab message, contains %zu entries", nextHopsMap->size());

			NodeNum ourNodeId = nodeDB->getNodeNum();

			meshtastic_CrisislabMessage_NextHops ourNextHops = meshtastic_CrisislabMessage_NextHops_init_default;

			for (const auto &entry : *nextHopsMap) {
				if (entry.first == ourNodeId) {
					ourNextHops = (meshtastic_CrisislabMessage_NextHops)entry.second;
					break;
				}
			}

			if (ourNextHops.node_ids_count == 0) {
				LOG_ERROR("No next hops list found for our node (%u)", ourNodeId);
				break;
			}

			LOG_DEBUG("printing next hops list");
			for (int i = 0; i < ourNextHops.node_ids_count; i++) {
				LOG_DEBUG("Next hop %d: %u", i, ourNextHops.node_ids[i]);
			}

			Preferences preferences;
			preferences.begin("crisislab", false);

			preferences.putBytes("next_hops", ourNextHops.node_ids, ourNextHops.node_ids_count * sizeof(uint32_t));

			preferences.end();

			break;
		}
		case meshtastic_CrisislabMessage_start_live_data_tag: {
			LOG_INFO("Handling \"start live data\" command");

			if (this->liveDataTaskHandle != nullptr) {
				LOG_WARN("Live data task already running, ignoring start live data command");
			}

			xTaskCreate(
				CrisislabCommon::sendLiveData,
				"LIVE_DATA", // task name
				8192, // stack size in words
				this, // task parameter
				5, // priority from 0-9
				&this->liveDataTaskHandle
			);

			break;
		}
		case meshtastic_CrisislabMessage_stop_live_data_tag: {
			LOG_INFO("Handling \"stop live data\" command");

			if (this->liveDataTaskHandle == nullptr) {
				LOG_WARN("Live data task not running, ignoring stop live data command");
			} else {
				LOG_DEBUG("Stopping live data task and setting handle to nullptr");
				vTaskDelete(this->liveDataTaskHandle);
				this->liveDataTaskHandle = nullptr;
			}

			break;
		}
		case meshtastic_CrisislabMessage_live_data_tag: {
#if MESHTASTIC_CRISISLAB_GATEWAY
			LOG_INFO("Handling \"live data\" from node %u", meshPacket->from);

			if (meshPacket == nullptr) {
				LOG_ERROR("Received live data without mesh packet");
				break;
			}

			GatewayModule::tryMqttPublish(
				meshPacket->decoded.payload.bytes,
				meshPacket->decoded.payload.size
			);
#else
			LOG_ERROR("We've somehow ended up with a live data packet in handleCrisislabMessage as a normal node. This should never happen as normal nodes should forward live data immediately");
#endif

			break;
		}
		default: {
			LOG_ERROR("Unknown (or unimplemented) crisislab message type: %d", message.which_message);
		}
	}
}

ProcessMessage CrisislabCommon::handleReceived(const meshtastic_MeshPacket &meshPacket) {
	// LOG_DEBUG("Received mesh packet from %u on port %d",
	// 	meshPacket.from,
	// 	meshPacket.decoded.portnum
	// );

	// if it's a normal message handle accordingly depending on who we are
	if (meshPacket.decoded.portnum == this->primaryPortNum) {
#if MESHTASTIC_CRISISLAB_GATEWAY
		gatewayModule->handleNormalMeshPacket(meshPacket);
#else
		normalNodeModule->handleNormalMeshPacket(meshPacket);
#endif

		return ProcessMessage::STOP;
	}

	// if we're a normal node, we're responsible for forwarding live data packets
	if (meshPacket.decoded.portnum == this->livePortNum) {
#if MESHTASTIC_CRISISLAB_NORMAL
		LOG_DEBUG("Node %u received live data packet", nodeDB->getNodeNum());

		Preferences preferences;
		preferences.begin("crisislab", true);

		if (!preferences.isKey("next_hops")) {
			LOG_ERROR("No next hops found, cannot forward live data packet so it will be dropped");
			preferences.end();
			return ProcessMessage::STOP;
		}

		const size_t nextHopsBufferLength = preferences.getBytesLength("next_hops");

		if (nextHopsBufferLength == 0) {
			LOG_ERROR("Next hops buffer length is 0, cannot forward live data packet so it will be dropped");
			preferences.end();
				return ProcessMessage::STOP;
		}

		uint32_t bestNextHop;
		preferences.getBytes("next_hops", &bestNextHop, sizeof(uint32_t));
		preferences.end();

		meshtastic_MeshPacket toForward;
		mempcpy(
			&toForward,
			&meshPacket,
			sizeof(meshtastic_MeshPacket)
		);

		toForward.to = bestNextHop;
		toForward.from = nodeDB->getNodeNum();
		toForward.channel = this->channelIndex;

		service->sendToMesh(&toForward);
#else
		gatewayModule->handleNormalMeshPacket(meshPacket);
#endif
	}

	return ProcessMessage::CONTINUE;
}

void CrisislabCommon::sendLiveData(void *params) {
	CrisislabCommon *self = (CrisislabCommon *)params;

	Preferences preferences;
	preferences.begin("crisislab", true);

	uint32_t bestNextHop = 0;

#if MESHTASTIC_CRISISLAB_NORMAL
	if (!preferences.isKey("next_hops")) {
		LOG_ERROR("No next hops found, cannot send live data, quitting task");
		preferences.end();
		vTaskDelete(NULL);
	}

	const size_t nextHopsBufferLength = preferences.getBytesLength("next_hops");

	if (nextHopsBufferLength == 0) {
		LOG_ERROR("Next hops buffer length is 0, cannot send live data, quitting task");
		preferences.end();
		vTaskDelete(NULL);
	}

	// get first next hop
	// TODO: support falling back to other next hops if the first one fails

	preferences.getBytes("next_hops", &bestNextHop, sizeof(uint32_t));
#endif

	if (!preferences.isKey("bcast_interval")) {
		LOG_ERROR("No broadcast interval found, cannot send live data, quitting task");
		preferences.end();
		vTaskDelete(NULL);
	}

	uint32_t broadcastIntervalSeconds = preferences.getUInt("bcast_interval" );

	while (true) {
		LOG_DEBUG("Sending live data packet");

		meshtastic_CrisislabMessage message = meshtastic_CrisislabMessage_init_default;
		message.which_message = meshtastic_CrisislabMessage_live_data_tag;
		message.message.live_data.node_num = nodeDB->getNodeNum();
		message.message.live_data.timestamp = self->secondsSinceEpoch();

		message.message.live_data.has_user = true;
		memcpy(
			&message.message.live_data.user,
			&owner,
			sizeof(meshtastic_User)
		);

		message.message.live_data.has_position = true;
		memcpy(
			&message.message.live_data.position,
			&localPosition,
			sizeof(meshtastic_Position)
		);

		message.message.live_data.has_device_metrics = true;
		const meshtastic_Telemetry telemetry = deviceTelemetryModule->getDeviceTelemetry();
		memcpy(
			&message.message.live_data.device_metrics,
			&telemetry.variant.device_metrics,
			sizeof(meshtastic_DeviceMetrics)
		);

		meshtastic_MeshPacket *meshPacket = self->allocMeshPacket(bestNextHop, self->livePortNum);

		meshPacket->decoded.payload.size = pb_encode_to_bytes(
			meshPacket->decoded.payload.bytes,
			sizeof(meshPacket->decoded.payload.bytes),
			&meshtastic_CrisislabMessage_msg,
			&message
		);

#if MESHTASTIC_CRISISLAB_NORMAL
		service->sendToMesh(meshPacket);
#else
		self->handleCrisislabMessage(message, meshPacket);
#endif

		vTaskDelay(broadcastIntervalSeconds * 1000 / portTICK_PERIOD_MS);
	}
}

void CrisislabCommon::returnSignalData(void *params) {
	CrisislabCommon *self = (CrisislabCommon *)params;

	Preferences preferences;
	preferences.begin("crisislab", true);

	if (!preferences.isKey("ping_timeout")) {
		LOG_ERROR("No ping collection timeout setting found, cannot return signal data, quitting task");
		preferences.end();
		vTaskDelete(NULL);
	}

	uint32_t pingCollectionTimeout = preferences.getUInt("ping_timeout", CrisislabCommon::pingCollectionTimeout);

	LOG_DEBUG("Starting timer to return signal data");

	vTaskDelay(pingCollectionTimeout / portTICK_PERIOD_MS);

	self->isCollectingPings = false;
	LOG_DEBUG("No longer collecting pings");

	meshtastic_CrisislabMessage message = meshtastic_CrisislabMessage_init_default;

	message.which_message = meshtastic_CrisislabMessage_signal_data_tag;
	message.message.signal_data.to = nodeDB->getNodeNum();
	message.message.signal_data.is_gateway = false;
	message.message.signal_data.links_count = self->signalDataEntries.size();

#if MESHTASTIC_CRISISLAB_GATEWAY
	message.message.signal_data.is_gateway = true;
#endif

	for (int i = 0; i < self->signalDataEntries.size(); i++) {
		memcpy(
			&message.message.signal_data.links[i],
			&self->signalDataEntries[i],
			sizeof(meshtastic_CrisislabMessage_SignalData_Entry)
		);
	}

	meshtastic_MeshPacket *meshPacket = self->allocMeshPacket();
	meshPacket->decoded.payload.size = pb_encode_to_bytes(
		meshPacket->decoded.payload.bytes,
		sizeof(meshPacket->decoded.payload.bytes),
		&meshtastic_CrisislabMessage_msg,
		&message
	);

#if MESHTASTIC_CRISISLAB_GATEWAY
	self->handleCrisislabMessage(message, meshPacket);
#else
	service->sendToMesh(meshPacket);
#endif

	LOG_DEBUG("Sent signal data to mesh (for gateway to publish to MQTT)");

	vTaskDelete(NULL);
}
