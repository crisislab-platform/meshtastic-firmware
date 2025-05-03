#pragma once

#include <Preferences.h>

#include "SinglePortModule.h"
#include "CrisislabCommon.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"

class NormalNodeModule : public CrisislabCommon
{
  public:
	NormalNodeModule();

  protected:
	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
};

