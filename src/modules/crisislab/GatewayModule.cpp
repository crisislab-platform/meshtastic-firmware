#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "MeshService.h"
#include "GatewayModule.h"
#include "mqtt/MQTT.h"
#include "../mesh/generated/meshtastic/crisislab.pb.h"

void GatewayModule::mqttCallback(char *topic, byte *payload, unsigned int length)
{
	LOG_INFO("GatewayModule got a message from MQTT on %s topic. It contains %d bytes.", topic, length);
	// if topic is "commands"
	if (strncmp(topic, "commands", 8) == 0) {
		meshtastic_CrisislabCommand cmd;

		if (!pb_decode_from_bytes(payload, length, &meshtastic_CrisislabCommand_msg, &cmd)) {
			LOG_ERROR("Failed to decode command message from MQTT");
			return;
		}

		if (cmd.type == meshtastic_CrisislabCommand_Type_SET_BROADCAST_INTERVAL) {
			LOG_INFO("Got command to set broadcast interval to %d seconds", cmd.payload.broadcast_interval_seconds);
		}
	}
}

int32_t GatewayModule::runOnce()
{
	if (mqtt == nullptr || !mqtt->isConnectedDirectly()) {
		LOG_DEBUG("Waiting for MQTT connection before altering default MQTT behavior");
		return 3000;
	} else {
		LOG_DEBUG("Got MQTT connection, changing default MQTT behavior");
		mqtt->setMqttCallback(mqttCallback);
		mqtt->subscribe("commands");
		return disable();
	}
}

ProcessMessage GatewayModule::handleReceived(const meshtastic_MeshPacket &packet)
{
	LOG_INFO("GatewayModule received a packet");
	return ProcessMessage::CONTINUE;
}

bool GatewayModule::wantPacket(const meshtastic_MeshPacket *packet)
{
    return MeshService::isTextPayload(packet);
}
