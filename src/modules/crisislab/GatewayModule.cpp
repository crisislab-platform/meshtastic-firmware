#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

#include "MeshService.h"
#include "GatewayModule.h"
#include "mqtt/MQTT.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "NormalNodeModule.h"
#include "common.h" // TODO: rename to CrisislabCommon

#define MQTT_INCOMING_TOPIC "for-mesh"
#define MQTT_INCOMING_TOPIC_LEN 8

// default to public channel
ChannelIndex GatewayModule::channelIndex = 0;

GatewayModule::GatewayModule() :
	SinglePortModule(
		"crisislabgatewaymodule",
		meshtastic_PortNum_CRISISLAB_GATEWAY_APP
	),
	concurrency::OSThread("GatewayModule")
{
	LOG_DEBUG("This node is a CRISiSLab gateway node");

	logPreferences({
		"bcast_interval",
		"channel_name",
	});

	// try to load channel index into memory for easy access

	Preferences preferences;
	preferences.begin("crisislab", true);

	if (preferences.isKey("channel_name")) {
		char channelName[12];
		preferences.getString("channel_name", channelName, 12);
		GatewayModule::channelIndex = channels.getByName(channelName).index;
	}

	preferences.end();
};

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

	meshtastic_MeshPacket *mesh_packet = router->allocForSending();

	mesh_packet->channel = GatewayModule::channelIndex;
	mesh_packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
	mesh_packet->decoded.portnum = meshtastic_PortNum_CRISISLAB_GATEWAY_APP;
	mesh_packet->decoded.payload.size = payloadLength;
	memcpy(mesh_packet->decoded.payload.bytes, payload, payloadLength);

	if (mesh_packet->channel == 0) {
		LOG_WARN("Crisislab modules are using the primary channel");
	} else {
		LOG_DEBUG("Using channel index %d for crisislab message", GatewayModule::channelIndex);
	}

	service->sendToMesh(mesh_packet);

	// handle the message ourselves accordingly
	handleCrisislabMessage(message);
}

int32_t GatewayModule::runOnce()
{
	if (mqtt == nullptr || !mqtt->isConnectedDirectly()) {
		LOG_DEBUG("Waiting for MQTT connection before altering default MQTT behavior");
		return 3000; // wait 3s before running runOnce again
	} else {
		LOG_DEBUG("Got MQTT connection, overriding default MQTT behavior");
		mqtt->setMqttCallback(mqttCallback);
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

bool GatewayModule::wantPacket(const meshtastic_MeshPacket *packet)
{
	// other gateways should not have any need to communicate with us so we only
	// care about packets from the normal nodes
	return packet->decoded.portnum == meshtastic_PortNum_CRISISLAB_NORMAL_APP;
}
