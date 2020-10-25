
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp32/pm.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "tef/led/NeoController.h"
#include "Animation/AnimationServer.h"
#include "Animation/Box.h"

#include "xasin/mqtt/Handler.h"

#include "esp_log.h"

led_coord_t led_data[ANIM_LED_COUNT] = {};

TEF::LED::NeoController leds = TEF::LED::NeoController(GPIO_NUM_15, RMT_CHANNEL_0, 20);

Xasin::MQTT::Handler mqtt = Xasin::MQTT::Handler();
TEF::Animation::AnimationServer server = TEF::Animation::AnimationServer();

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	Xasin::MQTT::Handler::try_wifi_reconnect(event);
	mqtt.wifi_handler(event);

	return ESP_OK;
}

void on_data(const std::string &topic, const void *data, size_t length) {
	auto str_data = reinterpret_cast<const char*>(data);

	if(server.parse_command(topic.data(), str_data))
		return;

	if(topic == "NEW") {
		if(strncmp(str_data, "BOX", 3) == 0) {
			int layer_no = 0;
			auto layer_ptr = strchr(str_data, ' ');
			if(layer_ptr != nullptr) {
				layer_ptr = strchr(layer_ptr + 1, ' ');

				if(layer_ptr != nullptr)
					layer_no = strtol(layer_ptr + 1, nullptr, 0);
			}

			auto new_box = new TEF::Animation::Box(server, server.decode_value_tgt(str_data).ID, leds.colours);

			new_box->x_coord = {1, 0};
			new_box->y_coord = {0, 1};
		}
	}
}

extern "C" void app_main(void)
{
	nvs_flash_init();
	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

	esp_timer_init();

	esp_pm_config_esp32_t power_config = {};
	power_config.max_freq_mhz = 80;
	power_config.min_freq_mhz = 80;
	power_config.light_sleep_enable = false;
	esp_pm_configure(&power_config);

	for(int i=0; i<ANIM_LED_COUNT; i++)
		led_data[i].x = i;

	TEF::Animation::AnimationElement::led_coordinates = led_data;

	Xasin::MQTT::Handler::start_wifi("TP-LINK_84CDC2\0", "f36eebda48\0");

	mqtt.start("mqtt://192.168.6.64");

	mqtt.subscribe_to("FurComs/Send/#", [](const Xasin::MQTT::MQTT_Packet data) {
		ESP_LOGI("TEST", "Got some info on %s, data %s", data.topic.data(), data.data.data());

		on_data(data.topic, data.data.data(), data.data.length());
	});

	while (true) {
		vTaskDelay(10);

		leds.colours.fill(0);

		server.tick(0.016);

		leds.update();
	}
}
