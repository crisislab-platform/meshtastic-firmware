#include "NormalNodeModule.h"
#include "CrisislabCommon.h"
#include "GatewayModule.h"

NormalNodeModule *normalNodeModule;

NormalNodeModule::NormalNodeModule() :
	CrisislabCommon("CrisislabNormalNode")
{
	LOG_DEBUG("This node is a normal CRISiSLab node");
}

void NormalNodeModule::handleNormalMeshPacket(const meshtastic_MeshPacket &packet)
{
	meshtastic_CrisislabMessage message;

	if (!this->decodeCrisislabMessageFromBytes(
		(byte *)packet.decoded.payload.bytes,
		packet.decoded.payload.size,
		&message
	)) {
		LOG_ERROR("Failed to decode crisislab message protobuf");
	} else {
		LOG_DEBUG("NormalNodeModule::handleReceived - decoded crisislab message");
		this->handleCrisislabMessage(message, &packet);
	}
}
