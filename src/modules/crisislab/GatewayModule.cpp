#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

#include "MeshService.h"
#include "GatewayModule.h"
#include "mqtt/MQTT.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "NormalNodeModule.h"
#include "CrisislabCommon.h"

#define MQTT_INCOMING_TOPIC "for-mesh"
#define MQTT_INCOMING_TOPIC_LEN 8

GatewayModule *gatewayModule;

GatewayModule::GatewayModule() :
	CrisislabCommon("CrisislabGateway"),
	concurrency::OSThread("CrisislabGateway")
{
	LOG_DEBUG("This node is a CRISiSLab gateway node");
}

// later we will override the mqtt callback so that this function is what's
// called every time we get a message from the mqtt broker
void GatewayModule::mqttCallback(char *topic, byte *payload, unsigned int payloadLength)
{
	LOG_DEBUG("GatewayModule got a message from MQTT on %s topic. It contains %d bytes.", topic, payloadLength);

	if (strncmp(topic, MQTT_INCOMING_TOPIC, MQTT_INCOMING_TOPIC_LEN) != 0) {
		LOG_DEBUG("GatewayModule ignoring message on topic %s", topic);
		return;
	}

	// all mqtt packets should be a protobuf of one type, which we decode here

	meshtastic_CrisislabMessage message;

	if (!pb_decode_from_bytes(payload, payloadLength, &meshtastic_CrisislabMessage_msg, &message)) {
		LOG_ERROR("Failed to decode command message from MQTT");
		return;
	}

	// propogate the message to the mesh

	meshtastic_MeshPacket *meshPacket = this->allocMeshPacketWithBytes(payload, payloadLength);

	if (meshPacket->channel == 0) {
		LOG_WARN("Crisislab modules are using the primary channel");
	} else {
		LOG_DEBUG("Using channel index %d for crisislab message", this->channelIndex);
	}

	service->sendToMesh(meshPacket);

	// handle the message ourselves accordingly
	this->handleCrisislabMessage(message, payload, payloadLength);
}

void GatewayModule::mqttCallbackStaticWrapper(char *topic, byte *payload, unsigned int length)
{
	gatewayModule->mqttCallback(topic, payload, length);
}

int32_t GatewayModule::runOnce()
{
	if (mqtt == nullptr || !mqtt->isConnectedDirectly()) {
		LOG_DEBUG("Waiting for MQTT connection before altering default MQTT behavior");
		return 3000; // wait 3s before running runOnce again
	} else {
		LOG_DEBUG("Got MQTT connection, overriding default MQTT behavior");
		mqtt->setMqttCallback(GatewayModule::mqttCallbackStaticWrapper);
		mqtt->subscribe("for-mesh");
		// disable this runOnce function
		return disable();
	}
}

ProcessMessage GatewayModule::handleReceived(const meshtastic_MeshPacket &packet)
{
	LOG_INFO("GatewayModule received a packet"); // temporary. just for testing.
	return ProcessMessage::CONTINUE;
}
