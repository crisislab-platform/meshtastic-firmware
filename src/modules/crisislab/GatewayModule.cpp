#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <pb_decode.h>

#include "MeshService.h"
#include "GatewayModule.h"
#include "mqtt/MQTT.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "NormalNodeModule.h"
#include "CrisislabCommon.h"

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

	if (!this->decodeCrisislabMessageFromBytes(payload, payloadLength, &message)) {
		LOG_ERROR("Failed to decode command message from MQTT");
		return;
	}

	LOG_DEBUG("GatewayModule decoded crisislab message protobuf from MQTT");

	// propogate the message to the mesh

	meshtastic_MeshPacket *meshPacket = this->allocMeshPacket();
	meshPacket->decoded.payload.size = payloadLength;
	memcpy(meshPacket->decoded.payload.bytes, payload, payloadLength);

	if (meshPacket->channel == 0) {
		LOG_WARN("Crisislab modules are using the primary channel");
	} else {
		LOG_DEBUG("Using channel index %d for crisislab message", this->channelIndex);
	}

	// broadcast message to the rest of the mesh to handle too
	service->sendToMesh(meshPacket);

	LOG_DEBUG("GatewayModule sent packet to mesh");

	if (message.which_message == meshtastic_CrisislabMessage_updated_routes_tag) {
		LOG_DEBUG("Gateway got updated routes. Map is at %p. Contains %zu entries", 
			CrisislabCommon::routesMapPtr.get(), CrisislabCommon::routesMapPtr->size());
	}

	// handle the message locally as well
	this->handleCrisislabMessage(message, meshPacket);
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

// publish to broker if we're connected, otherwise put in queue for when we do connect later
void GatewayModule::tryMqttPublish(const uint8_t *payload, size_t length) {
	if (moduleConfig.mqtt.proxy_to_client_enabled || mqtt->isConnectedDirectly()) {
		LOG_DEBUG("MQTT is connected, sending signal data");
		mqtt->publish( GatewayModule::MQTT_OUTGOING_TOPIC, payload, length, false);
	} else {
		LOG_INFO("MQTT not connected, queueing signal data packet");
		mqtt->enqueueMessage(std::string(GatewayModule::MQTT_OUTGOING_TOPIC), payload, length);
	}
}

ProcessMessage GatewayModule::handleReceived(const meshtastic_MeshPacket &packet)
{
	meshtastic_CrisislabMessage message;

	if (!this->decodeCrisislabMessageFromBytes(
		(byte *)packet.decoded.payload.bytes,
		packet.decoded.payload.size,
		&message
	)) {
		LOG_ERROR("Failed to decode crisislab message protobuf");
	} else {
		this->handleCrisislabMessage(message, &packet);
	}

	return ProcessMessage::STOP;
}
