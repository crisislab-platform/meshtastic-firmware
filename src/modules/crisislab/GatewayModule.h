#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

class GatewayModule : public SinglePortModule, private concurrency::OSThread
{
  public:
	GatewayModule() : SinglePortModule("crisislabgatewaymodule", meshtastic_PortNum_PRIVATE_APP), concurrency::OSThread("GatewayModule") {};

  protected:
	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *packet) override;
	virtual int32_t runOnce() override;

  private:
	static void mqttCallback(char *topic, byte *payload, unsigned int length);
};
