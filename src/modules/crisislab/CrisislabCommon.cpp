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
#include "mqtt/MQTT.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"

/* bool CrisislabCommon::decodeRoutesMapEntries(
	pb_istream_t *stream,
	const pb_field_iter_t *field,
	void **arg
) {
	LOG_DEBUG("decodeRoutesMapEntries called with stream %p, field %p, arg %p. %d bytes left in stream",
		stream, field, arg, stream->bytes_left);
	while (stream->bytes_left) {
		meshtastic_CrisislabMessage_RoutesMap_EntriesEntry entry =
			meshtastic_CrisislabMessage_RoutesMap_EntriesEntry_init_zero;

		if (!pb_decode(stream, meshtastic_CrisislabMessage_RoutesMap_EntriesEntry_fields, &entry)) {
			LOG_ERROR("Failed to decode RoutesMap entry");
			return false;
		}

		LOG_DEBUG("Decoded RoutesMap entry with key %u and value %s",
			entry.key, entry.has_value ? "present" : "not present");

		if (arg == nullptr) {
			LOG_ERROR("decodeRoutesMapEntries called with null arg pointer");
			return false;
		} else if (*arg == nullptr) {
			LOG_ERROR("arg points to a null pointer in decodeRoutesMapEntries");
return false;
		}

		auto entries = static_cast<std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList> *>(*arg);

		LOG_DEBUG("Inserting routes entry with key %u",
			entry.key, entries);

		(*entries)[entry.key] = entry.value;
	}

	return true;
} */

/* bool CrisislabCommon::decodeCrisislabMessageFromBytes(
	byte *buffer,
	unsigned int bufferLength,
	meshtastic_CrisislabMessage *msg
) {
	if (!buffer) {
		LOG_ERROR("decodeCrisislabMessageFromBytes called with null buffer pointer");
		return false;
	}

	if (!msg) {
		LOG_ERROR("decodeCrisislabMessageFromBytes called with null message pointer");
		return false;
	}

	pb_istream_t stream = pb_istream_from_buffer(buffer, bufferLength);

	*msg = (meshtastic_CrisislabMessage)meshtastic_CrisislabMessage_init_zero;

	pb_wire_type_t wire_type;
    uint32_t tag;
    bool eof;

    size_t bytesLeftBackup = stream.bytes_left;
	void *stateBackup = stream.state;

	// decode tag to determine if we need to set a decode callback before decoding the union
    if (!pb_decode_tag(&stream, &wire_type, &tag, &eof)) {
		LOG_ERROR("Failed to decode tag from stream (trying to decode CrisislabMessage)");
		return false;
    }

    if (eof) {
		// see https://jpa.kapsi.fi/nanopb/docs/concepts.html#decoding-callbacks, this
		// is apparently protocol
		stream.bytes_left = 0;
		LOG_ERROR("Unexpected end of stream while decoding CrisislabMessage");
		return false;
    }

	stream.bytes_left = bytesLeftBackup;
	stream.state = stateBackup;

	msg->which_message = tag;

	if (tag == meshtastic_CrisislabMessage_updated_routes_tag) {
		this->routesMapPtr.reset(new std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList>());
		msg->message.updated_routes.entries.arg = this->routesMapPtr.get();
		msg->message.updated_routes.entries.funcs.decode = CrisislabCommon::decodeRoutesMapEntries;

		// log arg and *arg pointers
		LOG_DEBUG("decodeCrisislabMessageFromBytes: arg pointer is %p, *arg pointer is %p",
			msg->message.updated_routes.entries.arg, *static_cast<std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList> *>(msg->message.updated_routes.entries.arg));
	}

	if (!pb_decode(&stream, meshtastic_CrisislabMessage_fields, msg)) {
		LOG_ERROR("Failed to decode CrisislabMessage from remaining stream: %s", PB_GET_ERROR(&stream));
		return false;
	}

	// if (tag == meshtastic_CrisislabMessage_updated_routes_tag) {
	// 	// log arg and *arg pointers
	// 	LOG_DEBUG("decodeCrisislabMessageFromBytes: AFTER DECODING THE REST arg pointer is %p, *arg pointer is %p",
	// 		msg->message.updated_routes.entries.arg, *static_cast<std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList> *>(msg->message.updated_routes.entries.arg));
	// }

	return true;
} */

std::unique_ptr<std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList>> CrisislabCommon::routesMapPtr(
    new std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList>()
);

bool meshtastic_CrisislabMessage_RoutesMap_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field) {
	LOG_DEBUG("meshtastic_CrisislabMessage_RoutesMap_callback: running");

	if (ostream) {
		LOG_ERROR("CrisislabMessage_RoutesMap callback called with ostream, but this is not supported");
		return false;
	}

	if (!istream) {
		LOG_ERROR("CrisislabMessage_RoutesMap callback called with null istream");
		return false;
	}

	meshtastic_CrisislabMessage_RoutesMap_EntriesEntry entry;

	if (istream->bytes_left) {
		if (!pb_decode(istream, meshtastic_CrisislabMessage_RoutesMap_EntriesEntry_fields, &entry)) {
			LOG_ERROR("Failed to decode RoutesMap entry: %s", PB_GET_ERROR(istream));
			return false;
		}

		CrisislabCommon::routesMapPtr->insert(std::make_pair(entry.key, entry.value));
	} else {
		LOG_DEBUG("No bytes left in stream (decodeRoutesMapEntries)");
	}

	return true;
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
	SinglePortModule(name, meshtastic_PortNum_CRISISLAB_APP)
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

meshtastic_MeshPacket *CrisislabCommon::allocMeshPacket(NodeNum to)
{
	meshtastic_MeshPacket *meshPacket = router->allocForSending();

	meshPacket->to = to;
	meshPacket->from = nodeDB->getNodeNum();
	meshPacket->channel = this->channelIndex;
	meshPacket->priority = meshtastic_MeshPacket_Priority_RELIABLE;
	meshPacket->decoded.portnum = this->ourPortNum;

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
		case meshtastic_CrisislabMessage_update_routes_request_tag: {
			LOG_INFO("Handling \"update routes request\" command");

			if (this->ignoreUpdateRoutesPackets) {
				LOG_DEBUG("Ignoring update routes packet");
				break;
			}

			this->ignoreUpdateRoutesPackets = true;

			// create ping message to be broadcast

			meshtastic_CrisislabMessage pingMessage = meshtastic_CrisislabMessage_init_default;

			pingMessage.which_message = meshtastic_CrisislabMessage_ping_timestamp_tag;

			const auto sinceEpoch = std::chrono::system_clock::now()
				.time_since_epoch();
			const uint64_t timestamp = std::chrono::duration_cast
				<std::chrono::seconds>(sinceEpoch).count();

			pingMessage.message.ping_timestamp = timestamp;

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
		case meshtastic_CrisislabMessage_ping_timestamp_tag: {
			LOG_INFO("Handling \"ping timestamp\" command from node %u", meshPacket->from);

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
		case meshtastic_CrisislabMessage_updated_routes_tag: {
			LOG_INFO("Handling \"updated routes\" command");

#if MESHTASTIC_CRISISLAB_GATEWAY
			LOG_DEBUG("We are a gateway, no need to handle updated routes");
			break;
#endif

			auto *routesMap = CrisislabCommon::routesMapPtr.get();

			if (routesMap == nullptr) {
				LOG_ERROR("handleCrisislabMessage received updated routes with null routes map");
				break;
			}

			LOG_DEBUG("Got routes map object from crisislab message, contains %zu entries", routesMap->size());

			NodeNum ourNodeId = nodeDB->getNodeNum();

			meshtastic_CrisislabMessage_RoutesList ourRoutesList = meshtastic_CrisislabMessage_RoutesList_init_default;

			for (const auto &entry : *routesMap) {
				if (entry.first == ourNodeId) {
					ourRoutesList = (meshtastic_CrisislabMessage_RoutesList)entry.second;
					break;
				}
			}

			if (ourRoutesList.routes_count == 0) {
				LOG_ERROR("No routes found for our node %u", ourNodeId);
				break;
			}

			// log our routes

			for (int i = 0; i < ourRoutesList.routes_count; i++) {
				meshtastic_CrisislabMessage_Route route = ourRoutesList.routes[i];

				LOG_DEBUG("Route %d from %u:", i, ourNodeId);
				for (int j = 0; j < route.node_ids_count; j++) {
					LOG_DEBUG("  -> %u", route.node_ids[j]);
				}
			}

			// Preferences preferences;
			// preferences.begin("crisislab", false);
			//
			// preferences.putBytes("routes", ourRoutesList, sizeof(ourRoutesList));
			//
			// preferences.end();

			break;
		}
		default: {
			LOG_ERROR("Unknown (or unimplemented) crisislab message type: %d", message.which_message);
		}
	}
}

void CrisislabCommon::returnSignalData(void *params) {
	CrisislabCommon *self = (CrisislabCommon *)params;

	LOG_DEBUG("Starting timer to return signal data");
	vTaskDelay(CrisislabCommon::pingCollectionTimeout / portTICK_PERIOD_MS);

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
