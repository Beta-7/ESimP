#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define debounceRate 50
#define maxBetweenButtonPresses 5000
//Stuff needed for executing the LEDs
//  
bool ledState=false;
bool sequenceInProgress=false;
int currentTiming;
int targetTiming;

//Stuff needed for capturing the sequence
//
bool buttonSequenceStarted = false;
bool buttonState = false;
int buttonDelay = 0;



//Stuff neede for MQTT
const char* ssid = "DAVID";
const char* password = "daviddavid2606";
const char* mqtt_server = "broker.mqttdashboard.com";
const int mqtt_port = 1883;
const char* mqtt_sequence_output_topic = "esp198273981273/test/seq/in";
const char* mqtt_sequence_input_topic = "esp198273981273/test/seq/out";


//Stuff needed for queueueue
//Isn't fool proof, but the MQTT library will break before this. Good enough for me
class Queue{
  private:
  int timings[100];
  int head=0;
  int tail=0;
  int queueItems=0;
  public:
  Queue(){}
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
    Serial.print("Popping ");Serial.print(timings[tail]);Serial.print(" from position ");Serial.println(tail);
    queueItems--;
    return timings[tail++];
  }
  Serial.println("End of queue!");
  return 0;
}
};


Queue leds;
Queue button;

//
void executeSequence(int pin){
  if(!sequenceInProgress){  
    digitalWrite(pin, ledState);
    if(!leds.queueEmpty()){
      targetTiming=leds.queuePop();
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
    leds.queuePush(atoi(part));
    }
    part = strtok(NULL, ";");
  }
}

void sendQueueToMQTT(){
  String outputString="";
  while(!button.queueEmpty()){
    outputString += button.queuePop();
    outputString += ";";
  }
  Serial.println(outputString);
  client.publish(mqtt_sequence_output_topic, outputString.c_str());
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
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D6,INPUT_PULLUP);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  leds = Queue();
  button = Queue();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  executeSequence(LED_BUILTIN);
  // Serial.println(digitalRead(D6));

  //If the state hasn't been set and the button has been pressed set the state.
  if(!buttonSequenceStarted && !digitalRead(D6)){
    buttonDelay=millis();
    buttonSequenceStarted = true;
    buttonState = digitalRead(D6);
  }
  //If the time between button presses is less than 5 seconds debounce it and send it to the queue,
  //else send the queue contents through MQTT and reset the state.
  if(buttonSequenceStarted && digitalRead(D6)!= buttonState){
    buttonState = digitalRead(D6);
    if(millis()-buttonDelay < maxBetweenButtonPresses){
      if(millis() - buttonDelay > debounceRate){
          button.queuePush(millis()-buttonDelay);
      }
          buttonDelay = millis();
    }
    else{
    buttonSequenceStarted = false;
    sendQueueToMQTT();
    }
  }
  //If the sequence was started, the button isn't held down and the time between presses is more than the set
  //stop the sequence and send the queue contents through MQTT.
  if(buttonSequenceStarted && digitalRead(D6) && millis()-buttonDelay > maxBetweenButtonPresses){
      buttonSequenceStarted = false;
      sendQueueToMQTT();
  }

}