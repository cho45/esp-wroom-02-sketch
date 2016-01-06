#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>

// platformio lib install 66
#include <HttpClient.h>

#include <EEPROM.h>
#include "utils.h"

#include "gf.hpp"
#include "mcp3425.hpp"
#include "adt7410.hpp"
#include "mpl115a.hpp"

#include "config.h"

WiFiUDP UDP;
WiFiClient wifiClient;
HttpClient http(wifiClient);
GrowthForecastClient gf(http, GF_HOST, GF_USER, GF_PASS);

const int eeprom_address = 0;
struct {
	char SSID[255] = WIFI_SSID;
	char Password[255] = WIFI_PASS;
}  wifi_config;

bool startWifi(int timeout) {
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);

	Serial.println("Reading wifi_config");
//	EEPROM.get(eeprom_address, wifi_config);
	Serial.println("wifi_config:");
	Serial.print("SSID: ");
	Serial.print(wifi_config.SSID);
	Serial.print("\n");
	if (strlen(wifi_config.SSID) == 0) {
		Serial.println("SSID is not configured");
		return false;
	}

	WiFi.begin(wifi_config.SSID, wifi_config.Password);
	int time = 0;
	for (;;) {
		switch (WiFi.status()) {
			case WL_CONNECTED:
				Serial.println("connected!");
				WiFi.printDiag(Serial);
				Serial.print("IPAddress: ");
				Serial.println(WiFi.localIP());
				return true;
			case WL_CONNECT_FAILED:
				Serial.println("connect failed");
				return false;
		}
		delay(1000);
		Serial.print(".");
		time++;
		if (time >= timeout) {
			break;
		}
	}
	return false;
}

void setup_ota () {
	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// Hostname defaults to esp8266-[ChipID]
	// ArduinoOTA.setHostname("myesp8266");

	// No authentication by default
	// ArduinoOTA.setPassword((const char *)"123");

	ArduinoOTA.onStart([]() {
		Serial.println("Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("End");
		ESP.restart();
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		switch (error) {
			case OTA_AUTH_ERROR: Serial.println("Auth Failed"); break;
			case OTA_BEGIN_ERROR: Serial.println("Begin Failed"); break;
			case OTA_CONNECT_ERROR: Serial.println("Connect Failed"); break;
			case OTA_RECEIVE_ERROR: Serial.println("Receive Failed"); break;
			case OTA_END_ERROR: Serial.println("End Failed"); break;
		}
	});
	ArduinoOTA.begin();
}

//void udp_log(char[] str) {
//	UDP.beginPacket("192.168.0.5", 5140);
//	UDP.write(str);
//	UDP.endPacket();
//}



MCP3425 mcp3425;
MPL115A mpl115a;
ADT7410 adt7410;

void setup() {
	Serial.begin(9600);

	pinMode(13, OUTPUT);

	Serial.println("Initializing...");
	// EEPROM.put(eeprom_address, wifi_config);
	if (startWifi(30)) {
		setup_ota();
		UDP.begin(12345);
	} else {
		Serial.println("failed to start wifi");
		ESP.restart();
	}

	// IO4 = SDA, IO5 = SCL
	Wire.begin();

	mcp3425.configure(MCP3425::ONESHOT, MCP3425::SAMPLE_RATE_240SPS, MCP3425::PGA_GAIN_1);
	adt7410.configure(ADT7410::ONE_SPS, ADT7410::RES_13BIT);
	mpl115a.initCoefficient();

	Serial.print("a0 = ");
	Serial.println(mpl115a.a0);
	Serial.print("b1 = ");
	Serial.println(mpl115a.b1);
	Serial.print("b2 = ");
	Serial.println(mpl115a.b2);
	Serial.print("c12 = ");
	Serial.println(mpl115a.c12);

}

void loop() {
	ArduinoOTA.handle();

/*
	// 10bit / 0 - 1.0V
	// E(V) = analogRead(A0) / (1<<10) * 1.0
	uint16_t read = analogRead(A0);
//	float v = (float)read / (1<<10) * 1.0;
//	Serial.printf("analogRead = %u\n", read);
//	Serial.println(String(v));

	digitalWrite(13, HIGH);
	post_to_gf("/home/test/espadc", read);
	digitalWrite(13, LOW);
*/

	float temp = adt7410.read();
	Serial.print("adt7410 = ");
	Serial.println(temp);
	gf.post("/home/test/temp", (int32_t)(temp * 1000));

	float hPa = mpl115a.calc_hPa();
	Serial.print("hPa = ");
	Serial.println(hPa);
	gf.post("/home/test/hPa", (int32_t)(hPa * 1000));

	float adc = mcp3425.read();
	Serial.print("mcp3425 = ");
	Serial.println(adc);

	digitalWrite(13, HIGH);
	delay(1000);
	digitalWrite(13, LOW);
}


void serialEvent() {
	while (Serial.available()) {
		char inChar = (char)Serial.read();
		Serial.print(inChar);
	}
}
