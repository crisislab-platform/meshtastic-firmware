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

	// Handle the message locally first. Pass nullptr for meshPacket because
	// MQTT inbound messages have no meaningful radio metadata (rssi/snr/from)
	// and to avoid use-after-free if we hand the packet to sendToMesh below.
	this->handleCrisislabMessage(message, nullptr);

	// broadcast message to the rest of the mesh to handle too
	if (message.which_message != meshtastic_CrisislabMessage_get_mesh_settings_request_tag) {
		service->sendToMesh(meshPacket); // ownership transferred; do not touch meshPacket after this
		LOG_DEBUG("GatewayModule sent packet to mesh");
	} else {
		// We allocated but didn't hand off; release back to the pool.
		packetPool.release(meshPacket);
	}
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

// Always enqueue. PubSubClient is not thread-safe; only the MQTT OSThread may
// call pubSub.publish/loop. The MQTT thread drains this queue every tick.
void GatewayModule::tryMqttPublish(const uint8_t *payload, size_t length) {
	mqtt->enqueueMessage(std::string(GatewayModule::MQTT_OUTGOING_TOPIC), payload, length);
}

void GatewayModule::handleNormalMeshPacket(const meshtastic_MeshPacket &packet)
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
}
