#include "common.h"
#include "NormalNodeModule.h"

NormalNodeModule::NormalNodeModule() :
	SinglePortModule("NormalNodeModule", meshtastic_PortNum_CRISISLAB_NORMAL_APP)
{
	LOG_DEBUG("This node is a normal CRISiSLab node");

	logPreferences({
		"bcast_interval",
	});
}

ProcessMessage NormalNodeModule::handleReceived(const meshtastic_MeshPacket &packet)
{
	meshtastic_CrisislabMessage message;

	if (!pb_decode_from_bytes(
		packet.decoded.payload.bytes,
		packet.decoded.payload.size,
		&meshtastic_CrisislabMessage_msg,
		&message
	)) {
		LOG_ERROR("Failed to decode crisislab message protobuf");
	} else {
		handleCrisislabMessage(message);
	}

	return ProcessMessage::STOP;
}

bool NormalNodeModule::wantPacket(const meshtastic_MeshPacket *packet)
{
	return packet->decoded.portnum == meshtastic_PortNum_CRISISLAB_NORMAL_APP ||
		packet->decoded.portnum == meshtastic_PortNum_CRISISLAB_GATEWAY_APP;
}
