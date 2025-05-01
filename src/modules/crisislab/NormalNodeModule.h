#pragma once

#include <Preferences.h>

#include "SinglePortModule.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"

class NormalNodeModule : public SinglePortModule
{
  public:
	NormalNodeModule();

  protected:
	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *packet) override;
};

