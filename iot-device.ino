#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <MQTT.h>

//Definitions

#define PIN_RESET 5
#define LED 2
#define TRIG 14 //GPIO Number 14, D5
#define ECHO 12 //GPIO Number 12, D6

#define STRING_LEN 128
#define NUMBER_LEN 32
#define CONFIG_VERSION "v7"

String clientMac = "";
unsigned char mac[6];

char payload[50];
char topic[150];

unsigned long previousTimeReading = 0;
unsigned long previousTimePublishing = 0;

const char thingName[] = "Logique Device AP";
const char wifiInitialApPassword[] = "11235813";

// Method declaration

void handleRoot();
void mqttMessageReceived(String &topic, String &payload);
bool connectMqtt();
bool connectMqttOptions();

// ********************

DNSServer dnsServer;
WebServer server(80);
WiFiClient net;
MQTTClient mqttClient;

int distance = 0;
float distanceFilterd = 0.0;
float percent = 0;

char mqttServer[STRING_LEN];
char deviceLabel[STRING_LEN];

char distMin[STRING_LEN];
char distMax[STRING_LEN];

char deviceVariable[STRING_LEN];
char deviceToken[STRING_LEN];
char portValue[NUMBER_LEN];
char sendPeriodValue[NUMBER_LEN];
char aquisitionPeriodValue[NUMBER_LEN];
char lamdaFilter[NUMBER_LEN];
char platformParamValue[STRING_LEN];
char sendDistanceValue[STRING_LEN];

static char pltformNames[][STRING_LEN] = { "Ubidots", "Thingsboard" };
static char pltformValues[][STRING_LEN] = { "ubidots", "thingsboard" };

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

IotWebConfParameterGroup groupMqtt = IotWebConfParameterGroup("mqtt_server", "MQTT Server");
IotWebConfParameterGroup groupTank = IotWebConfParameterGroup("tank", "Reservatório");
IotWebConfParameterGroup groupAcquisition = IotWebConfParameterGroup("acquisition", "Aquisição de dados"); 

IotWebConfTextParameter serverParam = IotWebConfTextParameter("Server", "mqttServer", mqttServer, STRING_LEN, "iot-logique.duckdns.org");
IotWebConfNumberParameter portParam = IotWebConfNumberParameter("Port", "mqttPort", portValue, NUMBER_LEN, "1883", "1..65535", "min='1' max='65535' step='1'");
IotWebConfSelectParameter iotPlatform = IotWebConfSelectParameter("Platform", "platformParam", platformParamValue, STRING_LEN, (char*)pltformValues, (char*)pltformNames, sizeof(pltformValues) / STRING_LEN, STRING_LEN);
IotWebConfCheckboxParameter sendRawDistance = IotWebConfCheckboxParameter("Enviar distância (cm)", "sendRawDistance", sendDistanceValue, STRING_LEN,  true);

IotWebConfNumberParameter aquisitionPeriod = IotWebConfNumberParameter("Aquisition period (s)", "aquisitionPeriod", aquisitionPeriodValue, NUMBER_LEN, "5", "1..3600", "min='1' max='3600' step='1'");
IotWebConfNumberParameter sendPeriod = IotWebConfNumberParameter("Send period (s)", "sendPeriod", sendPeriodValue, NUMBER_LEN, "60", "10..3600", "min='1' max='3600' step='1'");
IotWebConfNumberParameter lambda = IotWebConfNumberParameter("Filter (lambda)", "lambda", lamdaFilter, NUMBER_LEN, "0.8", "0..1", "min='0' max='10' step='0.001'");

IotWebConfTextParameter deviceLabelParam = IotWebConfTextParameter("Device Label", "deviceLabel", deviceLabel, STRING_LEN, "arnaldo-caixa-superior");
IotWebConfTextParameter deviceVariableParam = IotWebConfTextParameter("Device variable", "deviceVariable", deviceVariable, STRING_LEN, "level");
IotWebConfTextParameter deviceTokenParam = IotWebConfTextParameter("Device Token", "deviceToken", deviceToken, STRING_LEN, "arnaldo-caixa-superior-9786");

IotWebConfTextParameter minDistance = IotWebConfNumberParameter("Distância mínima", "distMin", distMin, NUMBER_LEN, "25", "10..400", "min='10' max='400' step='1'");
IotWebConfTextParameter maxDistance = IotWebConfNumberParameter("Distância máxima", "distMax", distMax, NUMBER_LEN, "200", "10..400", "min='10' max='400' step='1'");

bool needMqttConnect = false;
bool needReset = false;

unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;

void callback(char* topic, byte* payload, unsigned int length){ }

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");
  setupIotConfig();
  setupMqtt();
  setupLeds();
  Serial.println("Ready!");
}

void setupLeds(){
  pinMode(LED_BUILTIN, OUTPUT);
}

void blinkLed(int ms){
  digitalWrite(LED_BUILTIN, LOW);
  delay(ms); 
  digitalWrite(LED_BUILTIN, HIGH);
  delay(ms);
}

void setupMqtt() {
  WiFi.macAddress(mac);
  clientMac += macToStr(mac);
  mqttClient.begin(mqttServer, atoi(portValue), net);
  configureTopic();
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT_PULLUP);
  Serial.printf("MQTT configured! platform: %s topic: %s sendDistance: %s \n", platformParamValue, topic, sendRawDistance.isChecked() ? "true" : "false");
}

void configureTopic(){
  if (strcmp(platformParamValue, "ubidots") == 0){
    sprintf(topic, "%s%s", "/v1.6/devices/", deviceLabel);
  }else if (strcmp(platformParamValue, "thingsboard") == 0){
    sprintf(topic, "%s", "v1/devices/me/telemetry");
  }
}

void setupIotConfig(){
  
  groupMqtt.addItem(&iotPlatform);
  groupMqtt.addItem(&serverParam);
  groupMqtt.addItem(&portParam);
  groupMqtt.addItem(&deviceLabelParam);
  groupMqtt.addItem(&deviceVariableParam);
  groupMqtt.addItem(&deviceTokenParam);
  groupMqtt.addItem(&sendRawDistance);

  groupTank.addItem(&minDistance);
  groupTank.addItem(&maxDistance);

  groupAcquisition.addItem(&aquisitionPeriod);
  groupAcquisition.addItem(&sendPeriod);
  groupAcquisition.addItem(&lambda);
  
  iotWebConf.addParameterGroup(&groupMqtt);
  iotWebConf.addParameterGroup(&groupAcquisition);
  iotWebConf.addParameterGroup(&groupTank);

  iotWebConf.setWifiConnectionTimeoutMs(60000);
  iotWebConf.setApTimeoutMs(60000);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  
  iotWebConf.init();
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  
}

String macToStr(const uint8_t* mac) { 
  String result; 
  for (int i = 0; i < 6; ++i) { 
      result += String(mac[i], 16); 
      if (i < 5) result += ':';
  } 
  return result; 
}

void loop() {

  setupNetwork();

  unsigned long currentTime = millis();
  int sPeriod = atoi(sendPeriodValue) * 1000;
  int aPeriod = atoi(aquisitionPeriodValue) * 1000;
  
  if (networkIsOK()) {
      
    if(currentTime - previousTimeReading >= aPeriod) {
        previousTimeReading = currentTime;
        readData();
        blinkLed(30);
    }
    
    if(currentTime - previousTimePublishing >=  sPeriod){
      previousTimePublishing = currentTime;
      if (sendRawDistance.isChecked()){
        publishData("distance", distanceFilterd);
      }
      publishData(deviceVariable, percent);
    }
    
  }

}

void readData(){
    float lambda = atof(lamdaFilter);
    distance = loopLeitura();        
    distanceFilterd =  (lambda * distance) + (1 - lambda) * distanceFilterd;
    percent = 100 - (((distanceFilterd-atoi(distMin))*100)/(atoi(distMax)-atoi(distMin)));
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;
//  Serial.printf("Distance=%d - Filter= %f - Percent: %f \n", distance, distanceFilterd, percent);
}

void publishData(char *variable, float data){
  sprintf(payload, "{\"%s\": %f}", variable, data);
  mqttClient.publish(topic, payload);
//  Serial.printf("Published=%s on topic %s\n", payload, topic);
  blinkLed(300);
}

boolean networkIsOK(){
  return iotWebConf.getState() == iotwebconf::OnLine && mqttClient.connected();
}

int loopLeitura(){
  digitalWrite(TRIG, LOW); 
  delayMicroseconds(2); 
  // Send a 20uS high to trigger ranging
  digitalWrite(TRIG, HIGH);  
  delayMicroseconds(20);
  // Send pin low again
  digitalWrite(TRIG, LOW);
  int distance = pulseIn(ECHO, HIGH, 26000);
  distance = distance / 58;
  return distance;
}

void setupNetwork(){
  iotWebConf.doLoop();
  mqttClient.loop();
  if (needMqttConnect) {
    if (connectMqtt()) {
      needMqttConnect = false;
    }
  }else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient .connected())){
    Serial.println("MQTT reconnect");
    connectMqtt();
  }
  if (needReset){
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }
}

void wifiConnected(){
  needMqttConnect = true;
}

void configSaved(){
  Serial.println("Configuration was updated.");
  needReset = true;
}

bool connectMqtt() {
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt){
    return false;
  }
  
  Serial.print("Connecting to MQTT server [");
  Serial.print(mqttServer);
  Serial.print("/");
  Serial.print(portValue);
  Serial.println("]");
  
  bool isConnected = mqttClient.connect(mqttServer, deviceToken, NULL, false);
  if (!isConnected) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  Serial.println("MQTT Connected!");
  return true;
}


void handleRoot() {
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal()) return;

  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 01 Minimal</title></head><body>";
  s += "Go to <a href='config'>configure page</a> to change settings.";
  s += "</body></html>\n";
  server.send(200, "text/html", s);
}