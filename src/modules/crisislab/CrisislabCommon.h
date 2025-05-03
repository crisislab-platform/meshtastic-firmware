#pragma once

#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

class CrisislabCommon : public SinglePortModule
{
  public:
	CrisislabCommon(const char *name);

  protected:
	ChannelIndex channelIndex = 0;

	bool isUpdatingRoutes = false;

	meshtastic_MeshPacket *allocMeshPacket();

	meshtastic_MeshPacket *allocMeshPacketWithBytes(const byte *bytes, const size_t length);

	// for now this is the one function that will be called to handle each message,
	// later it may need to be split into multiple functions.
	void handleCrisislabMessage(
		const meshtastic_CrisislabMessage &message,
		const byte *payload,
		const size_t payloadLength
	);
};
