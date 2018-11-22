#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

const int led = D6;
const int NUMPIXELS = 1;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, led,
NEO_GRB + NEO_KHZ800);

const size_t bufferSize = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4);

typedef enum {
	defaultTopic, announce, deviceStatus, set, acknoledgement,
} Topic;

struct RGBColor {
	double red;       // a fraction between 0 and 1
	double green;       // a fraction between 0 and 1
	double blue;       // a fraction between 0 and 1

	static RGBColor hsv2rgb(double hue, double saturation, double brightness) {
		double hh, p, q, t, ff;
		long i;
		RGBColor out;

		if (saturation <= 0.0) {       // < is bogus, just shuts up warnings
			out.red = brightness;
			out.green = brightness;
			out.blue = brightness;
			return out;
		}
		hh = hue;
		if (hh >= 360.0)
			hh = 0.0;
		hh /= 60.0;
		i = (long) hh;
		ff = hh - i;
		p = brightness * (1.0 - saturation);
		q = brightness * (1.0 - (saturation * ff));
		t = brightness * (1.0 - (saturation * (1.0 - ff)));

		switch (i) {
		case 0:
			out.red = brightness;
			out.green = t;
			out.blue = p;
			break;
		case 1:
			out.red = q;
			out.green = brightness;
			out.blue = p;
			break;
		case 2:
			out.red = p;
			out.green = brightness;
			out.blue = t;
			break;

		case 3:
			out.red = p;
			out.green = q;
			out.blue = brightness;
			break;
		case 4:
			out.red = t;
			out.green = p;
			out.blue = brightness;
			break;
		case 5:
		default:
			out.red = brightness;
			out.green = p;
			out.blue = q;
			break;
		}

		Serial.println();
		Serial.print("RED -> ");
		Serial.print(out.red);
		Serial.println();
		Serial.print("GREEN -> ");
		Serial.print(out.green);
		Serial.println();
		Serial.print("BLUE-> ");
		Serial.print(out.blue);
		Serial.println();

		out.red = out.red * 255;
		out.green = out.green * 255;
		out.blue = out.blue * 255;
		Serial.println();
		Serial.print("RED -> ");
		Serial.print(out.red);
		Serial.println();
		Serial.print("GREEN -> ");
		Serial.print(out.green);
		Serial.println();
		Serial.print("BLUE-> ");
		Serial.print(out.blue);
		Serial.println();
		return out;
	}
};

struct WiFiConfig {
	static constexpr const char *ssid = "";
	static constexpr const char * pass = "";
};

struct MQTTConfig {
	static constexpr const char* server = "";
	static constexpr const char* username = "";
	static constexpr const char* password = "";
	static constexpr const char* clientID = "";
	static constexpr const int port = ;
};

struct MQTTTopic {
private:
	static constexpr const char *baseTopic = "lights";
	static constexpr const char* announce = "announce";
	static constexpr const char* deviceStatus = "deviceStatus";
	static constexpr const char* set = "update";
	static constexpr const char* acknoledgement = "acknoledgement";
public:
	static const char * topicPath(Topic topic) {

		switch (topic) {
		case Topic::deviceStatus:
			return "lights/POPULED/deviceStatus";
			break;

		case Topic::announce:
			return "lights/POPULED/announce";
			break;

		case Topic::set:
			return "lights/POPULED/update";
			break;

		case Topic::acknoledgement:
			return "lights/POPULED/acknoledgement";
			break;

		default:
			return "lights/POPULED";
			break;
		}
	}
};

struct Color {
	double hue = 0;
	double saturation = 0;
	double brightness = 0;
};

struct Light {
public:
	bool state = false;
	int index = 0;
	Color color;

	static Light model(String message) {

		DynamicJsonBuffer jsonBuffer(bufferSize);
		JsonObject& json = jsonBuffer.parseObject(message);
		Light newLight;
		if (!json.success()) {
			Serial.println("Failed to parse json");
			return newLight;
		}

		newLight.state = json["state"].as<bool>();
		newLight.index = json["index"].as<int>();

		if (json["color"].is<JsonObject&>()) {

			JsonObject& color = json["color"];
			float hue = color["hue"].as<double>();
			newLight.color.hue = hue;

			float saturation = color["saturation"].as<double>();
			newLight.color.saturation = saturation;

			float brightness = color["brightness"].as<double>();
			newLight.color.brightness = brightness;

		}
		return newLight;
	}
	static Light current() {
		Light l;
		l.state = digitalRead(led) > 0 ? true : false;
		return l;
	}
	JsonObject* json() {

		DynamicJsonBuffer jsonBuffer(bufferSize);

		JsonObject& root = jsonBuffer.createObject();
		root["state"] = this->state;
		root["index"] = this->index;

		JsonObject& color = root.createNestedObject("color");

		color["hue"] = this->color.hue;
		color["saturation"] = this->color.saturation;
		color["brightness"] = this->color.brightness;
		return &root;
	}

	void reply(PubSubClient toClient, Topic topic) {
		if (!toClient.connected()) {
			Serial.println("Warning: client is not connected reconnecting");
			return;
		}
		const char* path = MQTTTopic::topicPath(topic);

		if (path == "") {
			Serial.println("ERROR: BLANK PATH");
			return;
		}
		JsonObject* acknoledgement = this->json();
		char message[bufferSize];
		acknoledgement->printTo(message);
		toClient.publish(path, message);
	}
};

WiFiClient wifiClient;
PubSubClient client(MQTTConfig::server, MQTTConfig::port, wifiClient);

// Funcs
void updateLight(Light light) {

	RGBColor color = RGBColor::hsv2rgb(light.color.hue, light.color.saturation,
			light.color.brightness);
	//GRB
	pixels.setPixelColor(light.index, color.red, color.green, color.blue);
	pixels.show();
	light.reply(client, deviceStatus);
}

void callback(char* topic, byte* payload, unsigned int length) {
	String message = (char*) payload;
	Serial.println("");
	Serial.print("========== ");
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	Serial.println(" ==========");
	Serial.println(message);

	const char * setTopic = MQTTTopic::topicPath(set);
	if (strcmp(topic, setTopic) == 0) {
		Light light = Light::model(message);
		Serial.println(" updating color");
		updateLight(light);
	}

	const char * statusTopic = MQTTTopic::topicPath(deviceStatus);
	if (strcmp(topic, statusTopic) == 0) {
		Light light = Light::current();
		light.reply(client, announce);
	}
}

void subscribeToBroker() {
	client.subscribe(MQTTTopic::topicPath(defaultTopic));
	client.subscribe(MQTTTopic::topicPath(deviceStatus));
	client.subscribe(MQTTTopic::topicPath(set));
}

bool connectWithMQTTBroker() {

	if (client.connect(MQTTConfig::clientID, MQTTConfig::username,
			MQTTConfig::password)) {
		Serial.println("Connected to MQTT Broker!");
		client.setCallback(callback);
		subscribeToBroker();
		Light::current().reply(client, announce);
		return true;
	}

	Serial.println("Connection to MQTT Broker failed...");
	return false;

}

void connectWithWifi() {

	Serial.print("Connecting to ");
	Serial.println(WiFiConfig::ssid);

	WiFi.begin(WiFiConfig::ssid, WiFiConfig::pass);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("WiFi connected");
}

bool setupConnections() {

	connectWithWifi();
	return connectWithMQTTBroker();
}

void reconnect() {

	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");

		if (connectWithMQTTBroker()) {
			Serial.println("connected");
		} else {
			delay(5000);
		}
	}
}

void setupDefaults() {
	pixels.begin();
}

//Run loop
void setup() {
	Serial.begin(115200);
	setupDefaults();
	setupConnections();
}

void loop() {
	if (!client.connected()) {
		reconnect();
	}
	client.loop();
}
