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

	const static constexpr char *MQTT_OUTGOING_TOPIC = "for-server";
	const static constexpr char *MQTT_INCOMING_TOPIC = "for-mesh";
	const static constexpr int MQTT_INCOMING_TOPIC_LEN = 8;

	static void tryMqttPublish(const uint8_t *payload, size_t length);

  protected:
	virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
	virtual int32_t runOnce() override;

  private:
	void mqttCallback(char *topic, byte *payload, unsigned int length);
	static void mqttCallbackStaticWrapper(char *topic, byte *payload, unsigned int length);
};

extern bool decodeRoutesMapEntries(pb_istream_t *stream, const pb_field_iter_t *field, void **arg);
extern bool decodeCrisislabMessageFromBytes(byte *buffer, unsigned int bufferLength, meshtastic_CrisislabMessage *msg);

extern GatewayModule *gatewayModule;
