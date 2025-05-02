#include <Preferences.h>

#include "configuration.h"
#include "Channels.h"
#include "CrisislabCommon.h"

// default to public channel
ChannelIndex CrisislabCommon::channelIndex = 0;

CrisislabCommon::CrisislabCommon()
{
	Preferences preferences;
	preferences.begin("crisislab", true);

	// log preferences

	std::string prefNames[] = {
		"bcast_interval",
		"channel_name",
	};

	for (const auto &pref : prefNames) {
		if (!preferences.isKey(pref.c_str())) {
			LOG_ERROR("Crisislab setting %s not found", pref.c_str());
			continue;
		}

		switch (preferences.getType(pref.c_str())) {
			case PT_U32:
				LOG_INFO("Crisislab setting: %s = %u", pref.c_str(),
			 		preferences.getUInt(pref.c_str()));
				break;
			case PT_STR:
				LOG_INFO("Crisislab setting: %s = %s", pref.c_str(),
			 		preferences.getString(pref.c_str()).c_str());
				break;
			default:
				LOG_WARN("Logging crisislab setting %s not implemented");
		}
	}

	// load channel index into memory
	if (preferences.isKey("channel_name")) {
		char channelName[12];
		preferences.getString("channel_name", channelName, 12);
		CrisislabCommon::channelIndex = channels.getByName(channelName).index;
	}

	preferences.end();
}

void CrisislabCommon::handleCrisislabMessage(const meshtastic_CrisislabMessage &message)
{
	Preferences preferences;

	switch (message.which_message) {
		case meshtastic_CrisislabMessage_broadcast_interval_seconds_tag:
			LOG_DEBUG("Handling \"set broadcast interval\" command");

			preferences.begin("crisislab", false);
			preferences.putUInt("bcast_interval", message.message.broadcast_interval_seconds);
			preferences.end();

			break;
		case meshtastic_CrisislabMessage_channel_name_tag:
			LOG_DEBUG("Handling \"set channel name\" command");

			preferences.begin("crisislab", false);
			preferences.begin("crisislab", false);
			preferences.putString("channel_name", message.message.channel_name);
			preferences.end();

			break;
		default:
			LOG_ERROR("Unknown (or unimplemented) crisislab message type: %d", message.which_message);
	}
}
