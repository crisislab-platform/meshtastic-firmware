#pragma once

#include <Preferences.h>

#include "ProtobufModule.h"
#include "CrisislabCommon.h"
#include "concurrency/OSThread.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"

class GatewayModule : public CrisislabCommon, private concurrency::OSThread
{
  public:
	GatewayModule();

  protected:
	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
	virtual int32_t runOnce() override;

  private:
	void mqttCallback(char *topic, byte *payload, unsigned int length);
	static void mqttCallbackStaticWrapper(char *topic, byte *payload, unsigned int length);
};

extern GatewayModule *gatewayModule;
