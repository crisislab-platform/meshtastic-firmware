#pragma once

#include <Preferences.h>

#include "ProtobufModule.h"
#include "CrisislabCommon.h"
#include "concurrency/OSThread.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"

class GatewayModule : public CrisislabCommon, public SinglePortModule, private concurrency::OSThread
{
  public:
	GatewayModule();

	static ChannelIndex channelIndex;

  protected:
	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *packet) override;
	virtual int32_t runOnce() override;

  private:
	static void mqttCallback(char *topic, byte *payload, unsigned int length);
};
