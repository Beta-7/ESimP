
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


//Stuff needed for executing the LEDs
//  
bool ledState=false;
bool sequenceInProgress=false;
int currentTiming;
int targetTiming;


//Stuff neede for MQTT
const char* ssid = "xxxxxxxxx";
const char* password = "x";
const char* mqtt_server = "18.197.171.34";
const int mqtt_port = 1883;
const char* mqtt_sequence_input_topic = "esp198273981273/test/seq/in";
const char* mqtt_sequence_output_topic = "esp198273981273/test/seq/out";

//Stuff needed for queueueue
int timings[100];
int head=0;
int tail=0;
int queueItems=0;
void queuePush(int num){
  if(head<100){
    Serial.print("Pushing ");Serial.print(num);Serial.print(" to position ");Serial.println(head);
    timings[head++]=num;
    queueItems++;
  }

}
bool queueEmpty(){
  if(queueItems==0){
    head=0;
    tail=0;
    ledState=true;
  }
  return queueItems==0;
}
int queuePop(){
  if(tail<100){
    Serial.print("Popping");Serial.print(timings[tail]);Serial.print(" from position ");Serial.println(tail);
    queueItems--;
    return timings[tail++];
  }
  Serial.println("End of queue!");
  return 0;
}

//
void executeSequence(int pin){
  if(!sequenceInProgress){  
    digitalWrite(BUILTIN_LED, ledState);
    if(!queueEmpty()){
      targetTiming=queuePop();
      sequenceInProgress=true;
      currentTiming=millis();
    }
  }
  else{
    if(millis() - currentTiming > targetTiming){
      ledState = !ledState;
      sequenceInProgress=false;
    }
  }
}

char* part;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char message[length];
  memcpy(message, payload, length);
  message[length]='\0';
  Serial.println(message);
  part = strtok(message, ";");
  ledState=false;
  while (part!=NULL){
     if(atoi(part)!=0){
    queuePush(atoi(part));
    }
    part = strtok(NULL, ";");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe(mqtt_sequence_input_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  executeSequence(BUILTIN_LED);
  delay(10);
}