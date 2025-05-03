#include "NormalNodeModule.h"
#include "CrisislabCommon.h"

NormalNodeModule::NormalNodeModule() :
	CrisislabCommon("CrisislabNormalNode")
{
	LOG_DEBUG("This node is a normal CRISiSLab node");
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
		CrisislabCommon::handleCrisislabMessage(
			message,
			packet.decoded.payload.bytes,
			packet.decoded.payload.size
		);
	}

	return ProcessMessage::STOP;
}
