#pragma once

#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include <vector>
#include <map>

class CrisislabCommon : public SinglePortModule
{
  public:
	CrisislabCommon(const char *name);

	static std::unique_ptr<std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList>> routesMapPtr;

  protected:
	ChannelIndex channelIndex = 0;

	bool ignoreUpdateRoutesPackets = false;
	bool isCollectingPings = false;
	const static unsigned int pingCollectionTimeout = 50000; // ms
	std::vector<meshtastic_CrisislabMessage_SignalData_Entry> signalDataEntries;

	// static std::unique_ptr<std::map<uint32_t, meshtastic_CrisislabMessage_RoutesList>> routesMapPtr;

	meshtastic_MeshPacket *allocMeshPacket(NodeNum to = NODENUM_BROADCAST);

	// for now this is the one function that will be called to handle each message,
	// later it may need to be split into multiple functions.
	void handleCrisislabMessage(
		const meshtastic_CrisislabMessage &message,
		const meshtastic_MeshPacket *meshPacket
	);

	static void returnSignalData(void *params);

	bool decodeCrisislabMessageFromBytes(
		byte *buffer,
		unsigned int bufferLength,
		meshtastic_CrisislabMessage *msg
	);
};
