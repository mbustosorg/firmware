#include "application.h"
#include "neopixel.h"

SYSTEM_MODE(MANUAL);

// Animation variables
#define PIXEL_COUNT (16)
#define PIXEL_PIN (D3)
#define PIXEL_TYPE (WS2812)
int currentPixel = 0;
int currentRed = 200;
int currentGrn = 0;
int currentBlu = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

// Communication variables
#define HEARTBEAT_INTERVAL (5000)
#define LISTEN_PORT (23)
uint8_t serverIP[] = { 10, 0, 1, 8 };
#define SERVER_PORT (8100)
#define MAC_ADDRESS_LENGTH (6)
uint8_t macAddress[MAC_ADDRESS_LENGTH];

//byte serverIP[] = { 74, 125, 224, 72 };
//#define SERVER_PORT (80)

TCPServer server = TCPServer(LISTEN_PORT);
TCPClient client;
TCPClient registerClient;
int led2 = D7; // This one is the built-in tiny one to the right of the USB jack

// Node Statemachine
typedef enum State {STARTUP, REGISTERING, OPERATION, DROPPED, CLOUD} State;
unsigned long stateTime = millis();
unsigned long lastHeartbeat = 0;
State currentState = STARTUP;
String macAddressString();

void setup()
{
	WiFi.connect();
	WiFi.macAddress(macAddress);
	Serial.begin(9600);
	while(!Serial.available()) SPARK_WLAN_Loop();

	printTimestamp();
	Serial.println(WiFi.localIP());
	printTimestamp();
	Serial.println(WiFi.subnetMask());
	printTimestamp();
	Serial.println(WiFi.gatewayIP());
	printTimestamp();
	Serial.println(WiFi.SSID());
	printTimestamp();
	Serial.println(macAddressString().c_str());

  	pinMode(PIXEL_PIN, OUTPUT);
	strip.begin();
	strip.show();
  	pinMode(led2, OUTPUT);
}

// Display current timestamp
void printTimestamp() {
	Serial.print(millis());
	Serial.print(": ");
}

// String object of our MAC Address
String macAddressString() {
	String macString = "";
	for (int i = 0; i < MAC_ADDRESS_LENGTH; i++) {
		macString += macAddress[i];
		if (i < MAC_ADDRESS_LENGTH - 1) macString += ":";
	}
	return macString;
}

// Print our IP address
void printController() {
	for (int i = 0; i < 4; i++) {
		Serial.print(serverIP[i]);
		if (i < 3) Serial.print(".");
	}
	Serial.print(":");
	Serial.println(SERVER_PORT);
}

// Register the node with the controller
void registerSelf() {
	if (registerClient.connect(serverIP, SERVER_PORT)) {
		server.begin();
		printTimestamp();
		Serial.print("Connected: ");
		printTimestamp();
		printController();
		registerClient.print("GET /register?");
		registerClient.print("id="); registerClient.print(macAddressString().c_str()); registerClient.print("&");
		registerClient.print("address="); registerClient.print(WiFi.localIP()); registerClient.print("&");
		registerClient.print("port="); registerClient.print(LISTEN_PORT);
		registerClient.println(" HTTP/1.0");
		registerClient.println("Host: www.google.com");
		registerClient.println("Content-Length: 0");
		registerClient.println();
		currentState = REGISTERING;
		stateTime = millis();
	} else {
		printTimestamp();
		Serial.print("Could not connect: ");
		printController();
	}
}

// Handle registration HTTP response
void handleHttpResponse() {
	if (registerClient.connected()) {
		while (registerClient.available()) {
			char c = registerClient.read();
			Serial.print(c);
		}
	}
}

// Wait for the controller to recognize our registration request
void awaitRecognition() {
	if (client.connected()) {
		while (client.available()) {
			char c = client.read();
			if (c == 'X') {
				printTimestamp();
				Serial.println("Recognized");
				currentState = OPERATION;
				stateTime = millis();
			}
		}
	} else {
		client = server.available();
	}
}

// Listen for cloud updates
void handleCloud() {
	if (Spark.connected() == false) {
		printTimestamp();
		Serial.println("Connecting to cloud");
		Spark.connect();
	}
	Spark.process();
}

// Normal operation
void handleCommands() {
	while (client.available()) {
		char c = client.read();
		printTimestamp();
		Serial.print("Input: ");
		Serial.println(c);
		if (c == 'P') {
			printTimestamp();
			Serial.println("Ping request");
			client.write('P');
		} else if (c == '1') {
			printTimestamp();
			Serial.println("Moving to cloud");
			currentState = CLOUD;
		} else if (c == '2') {
			printTimestamp();
			Serial.println("Heartbeat request");
			lastHeartbeat = 0;
		} else if (c == '3') {
			printTimestamp();
			Serial.println("Advance pixel request");
			currentPixel++;
			if (currentPixel >= PIXEL_COUNT) currentPixel = 0;
		} else if (c == 'R') {
			printTimestamp();
			Serial.println("Red");
			currentRed = 200;
			currentGrn = 0;
			currentBlu = 0;
		} else if (c == 'G') {
			printTimestamp();
			Serial.println("Green");
			currentRed = 0;
			currentGrn = 200;
			currentBlu = 0;
		} else if (c == 'B') {
			printTimestamp();
			Serial.println("Blue");
			currentRed = 0;
			currentGrn = 0;
			currentBlu = 200;
		}
	}
	if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
		printTimestamp();
		Serial.println("Heartbeat sent");
		lastHeartbeat = millis();
		client.write('h');
	}
}

// Normal LED control loop
void animationLoop() {
    for(int i = 0; i < strip.numPixels(); i++) {
    	if (currentPixel == i) {
    		strip.setPixelColor(i, currentRed, currentGrn, currentBlu);
    	} else {
    		strip.setPixelColor(i, 5, 0, 0);
    	}
    }
    strip.show();
}

void loop()
{
	handleHttpResponse();
	if (currentState == STARTUP) {
		registerSelf();
	} else if (currentState == REGISTERING) {
		awaitRecognition();
	} else if (currentState == OPERATION) {
		handleCommands();
		animationLoop();
	} else if (currentState == CLOUD) {
		handleCloud();
		handleCommands();
		animationLoop();
	}
}

// Protocol
// Node->Controller:
//    h: Heartbeat
//
// Controller->Node:
//    1: Enter cloud mode
//    2: Request heartbeat (e.g. ping)
//    3. Advance pixel
//    P: Ping
//    X: Recognized by Controller
//
// Pinout Diagram: https://docs.google.com/a/bustos.org/drawings/d/12OdPwacGCoI-6NfFAYS2rjWgYxKWuHG0zupCH1tGLEQ/edit?hl=en&forcehl=1
//
// ** Spark cli compile & deploy **
// spark compile tidesNode.ino 
// #Press mode then rst and release when yellow
// sudo dfu-util -l
// spark flash --usb firmware_*.bin 
// spark setup # WPA2
//
// ** spark-core lib build & deploy **
// [~/Documents/development/git/sparkio/core-firmware/build]$ make APP=tidesNode
// [~/Documents/development/git/sparkio/core-firmware/build]$ dfu-util -d 1d50:607f -a 0 -s 0x08005000:leave -D applications/tidesNode/tidesNode.bin
