#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "MeshService.h"
#include "GatewayModule.h"
#include "mqtt/MQTT.h"

void GatewayModule::mqttCallback(char *topic, byte *payload, unsigned int length)
{
	LOG_INFO("GatewayModule got a message from MQTT on %s topic. It contains %d bytes.", topic, length);
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
	mqtt->publish("data", "Hello from GatewayModule!!", true);
	return ProcessMessage::CONTINUE;
}

bool GatewayModule::wantPacket(const meshtastic_MeshPacket *packet)
{
    return MeshService::isTextPayload(packet);
}
