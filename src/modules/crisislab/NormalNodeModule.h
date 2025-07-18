#pragma once

#include <Preferences.h>

#include "CrisislabCommon.h"
#include "../../mesh/generated/meshtastic/crisislab.pb.h"

class NormalNodeModule : public CrisislabCommon
{
  public:
	NormalNodeModule();

	void handleNormalMeshPacket(const meshtastic_MeshPacket &packet);
};

extern NormalNodeModule *normalNodeModule;

