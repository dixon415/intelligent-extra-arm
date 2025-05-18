#define BROKER_IP    "x.x.x.x"
#define DEV_NAME     "client-name"
#define MQTT_USER    "user-name"
#define MQTT_PW      "user-password"
#define MQTT_MAX_PACKET_SIZE 1024

const char ssid[] = "your-ssid";
const char pass[] = "your-password";

#include <ArduinoJson.h>
#include <MQTT.h>
#include <WiFiNINA.h>
#include <map>
#include <string>
#include <algorithm>
#include <Servo.h>

WiFiClient net;
MQTTClient client;

struct Component {
    std::string label;
    float confidence;
    int posX;
    int posY;
};
std::map<std::string, Component> componentMap;

Servo gripperServo;  // Servo 1 (Gripper)
Servo armServo1;     // Servo 2 (Up/Down)
Servo armServo2;     // Servo 3 (Up/Down)
Servo baseServo;     // Servo 4 (Rotation)

const int gripperPin = 2;
const int arm1Pin = 3;
const int arm2Pin = 5;
const int basePin = 6;

const int basePositionA = 90;  // Rotation at Location A
int basePositionB = 0; // Rotation at Location B

const int arm1UpPosition = 130;   // Arm lifted
const int arm2UpPosition = 90;   // Arm lifted
const int arm1DownPosition = 90; // Arm lowered
const int arm2DownPosition = 130; // Arm lowered

const int gripperOpen = 135;  // Open gripper
const int gripperClosed = 90; // Close gripper

const int stepDelay = 10; // Delay between steps

void storeData(const char* topic, const char* jsonPayload) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, jsonPayload);
    
    Component comp;
    comp.label = doc["label"].as<std::string>();
    comp.confidence = doc["confidence"].as<float>();
    comp.posX = doc["position"]["x"].as<int>();
    comp.posY = doc["position"]["y"].as<int>();

    componentMap[comp.label] = comp;
}

Component getComponentData(const std::string& label) {
    if (componentMap.find(label) != componentMap.end()) {
        return componentMap[label];
    } else {
        return {"", 0.0, 0, 0};  // Return an empty component if not found
    }
}

void printComponentMap() {
    Serial.println("Stored components:");

    for (const auto& pair : componentMap) {
        Serial.print("Label: ");
        Serial.println(pair.second.label.c_str());

        Serial.print("Confidence: ");
        Serial.println(pair.second.confidence);

        Serial.print("Position: (");
        Serial.print(pair.second.posX);
        Serial.print(", ");
        Serial.print(pair.second.posY);
        Serial.println(")");
        
        Serial.println("---------------------");
    }
}

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting...");
  while (!client.connect(DEV_NAME, MQTT_USER, MQTT_PW)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
  client.subscribe("classification_topic");
  client.subscribe("alexa_topic");
}

void messageReceived(String &topic, String &payload) {
  //Serial.println("incoming: " + topic + " - " + payload);
  if (topic == "alexa_topic") {
    Serial.println("incoming: " + topic + " - " + payload);
    //printComponentMap();
    std::string cppString = payload.c_str();
    std::transform(cppString.begin(), cppString.end(), cppString.begin(), ::tolower);
    // Capitalize the first letter
    if (!cppString.empty()) {
        cppString[0] = toupper(cppString[0]);
    }
    Component retrieved = getComponentData(cppString);
    Serial.print("Label: ");
    Serial.println(retrieved.label.c_str());
    Serial.print("Confidence: ");
    Serial.println(retrieved.confidence);
    Serial.print("Position: (");
    Serial.print(retrieved.posX);
    Serial.print(", ");
    Serial.print(retrieved.posY);
    Serial.println(")");
    pickComponent(retrieved.posY);
  } else if (topic == "classification_topic") {
    storeData(topic.c_str(), payload.c_str());
  }
}

void gradualMove(Servo &servo, int startPos, int endPos) {
  int step = (startPos < endPos) ? 1 : -1;  // Determine direction
  for (int pos = startPos; pos != endPos; pos += step) {
      servo.write(pos);
      delay(stepDelay);
  }
  servo.write(endPos);  // Ensure final position is reached
}

int getBasePosition(int y) {
  if (y >= 0 && y <= 20) return 0;
  else if (y > 20 && y <= 40) return 10;
  else if (y > 40 && y <= 60) return 20;
  return 0;
}

void pickComponent(int y) {
  basePositionB = getBasePosition(y);
  // Move to Location B
  Serial.println("Moving to Location B...");
  baseServo.write(basePositionB);
  //gradualMove(baseServo, basePositionA, basePositionB);
  delay(1000);

  // Lower arm
  Serial.println("Lowering arm...");
  gradualMove(armServo1, arm1UpPosition, arm1DownPosition);
  gradualMove(armServo2, arm2UpPosition, arm2DownPosition);
  delay(1000);

  // Close gripper (pick item)
  Serial.println("Picking up item...");
  gradualMove(gripperServo, gripperOpen, gripperClosed);
  delay(1000);

  // Lift arm
  Serial.println("Lifting item...");
  armServo1.write(arm1UpPosition);
  armServo2.write(arm2UpPosition);
  //gradualMove(armServo1, arm1DownPosition, arm1UpPosition);
  //gradualMove(armServo2, arm2DownPosition, arm2UpPosition);
  delay(1000);

  // Move back to Location A
  Serial.println("Returning to Location A...");
  baseServo.write(basePositionA);
  //gradualMove(baseServo, basePositionB, basePositionA);
  delay(1000);

  // Lower arm
  Serial.println("Lowering arm...");
  gradualMove(armServo1, arm1UpPosition, arm1DownPosition);
  gradualMove(armServo2, arm2UpPosition, arm2DownPosition);
  delay(1000);

  // Open gripper (drop item)
  Serial.println("Dropping item...");
  gradualMove(gripperServo, gripperClosed, gripperOpen);
  delay(1000);

  // Lift arm back up
  Serial.println("Resetting arm...");
  armServo1.write(arm1UpPosition);
  armServo2.write(arm2UpPosition);
  //gradualMove(armServo1, arm1DownPosition, arm1UpPosition);
  //gradualMove(armServo2, arm2DownPosition, arm2UpPosition);
  delay(2000);
}

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, pass);
  client.begin(BROKER_IP, 1883, net);
  client.onMessage(messageReceived);
  connect();

  gripperServo.attach(gripperPin);
  armServo1.attach(arm1Pin);
  armServo2.attach(arm2Pin);
  baseServo.attach(basePin);

  baseServo.write(basePositionA);
  armServo1.write(arm1UpPosition);
  armServo2.write(arm2UpPosition);
  gripperServo.write(gripperOpen);

  delay(2000);
}

void loop() {
  client.loop();
  if (!client.connected()) {
    connect();
  }
}