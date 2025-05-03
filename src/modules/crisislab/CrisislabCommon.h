#pragma once

#include "../../mesh/generated/meshtastic/crisislab.pb.h"

class CrisislabCommon
{
  public:
	CrisislabCommon();

	static ChannelIndex channelIndex;

	static bool isUpdatingRoutes;

	// for now this is the one function that will be called to handle each message,
	// later it may need to be split into multiple functions.
	static void handleCrisislabMessage(
		const meshtastic_CrisislabMessage &message,
		const byte *payload,
		const size_t payloadLength
	);
};
