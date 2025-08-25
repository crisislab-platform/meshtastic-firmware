#pragma once

#include "../../mesh/generated/meshtastic/crisislab.pb.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include <vector>
#include <map>

class CrisislabCommon : public MeshModule
{
  public:
	CrisislabCommon(const char *name);

	static std::unique_ptr<std::map<uint32_t, meshtastic_CrisislabMessage_NextHops>> nextHopsMapPtr;

  protected:
	meshtastic_PortNum primaryPortNum = meshtastic_PortNum_CRISISLAB_APP_PRIMARY;
	meshtastic_PortNum livePortNum = meshtastic_PortNum_CRISISLAB_APP_LIVE;

	const static unsigned int pingCollectionTimeoutSeconds = 50;

    virtual bool wantPacket(const meshtastic_MeshPacket *p) override {
		return p->decoded.portnum == primaryPortNum
			|| p->decoded.portnum == livePortNum
			|| p->decoded.portnum == meshtastic_PortNum_ROUTING_APP;
	}

	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &meshPacket) override;

	ChannelIndex channelIndex = 0;
	bool ignoreUpdateNextHopsRequestPackets = false;
	bool isCollectingPings = false;
	std::vector<meshtastic_CrisislabMessage_SignalData_Entry> signalDataEntries;
	TaskHandle_t liveDataTaskHandle = nullptr;

	meshtastic_MeshPacket *allocMeshPacket(
		NodeNum to = NODENUM_BROADCAST,
		meshtastic_PortNum portNum = meshtastic_PortNum_CRISISLAB_APP_PRIMARY
	);

	uint64_t secondsSinceEpoch();

	// for now this is the one function that will be called to handle each message,
	// later it may need to be split into multiple functions.
	void handleCrisislabMessage(
		meshtastic_CrisislabMessage &message,
		const meshtastic_MeshPacket *meshPacket
	);

	static void sendLiveTelemetry(void *params);
	static void returnSignalData(void *params);

	bool decodeCrisislabMessageFromBytes(
		byte *buffer,
		unsigned int bufferLength,
		meshtastic_CrisislabMessage *msg
	);
};
