/* Authors: Vera T. Pascual, Antonio Roldan */
/*    Date: December 2023                   */

#include <WiFi.h> //Wifi library
#include "Adafruit_MQTT.h" // MQTT libraries
#include "Adafruit_MQTT_Client.h"
#include "user_credentials.h" // Personal credentials for 'eduroam' connection

#define EAP_ANONYMOUS_IDENTITY "20220719anonymous@urjc.es" // leave as it is

// Serial communication ports
#define RXD2 33
#define TXD2 4

//SSID NAME
const char* ssid = "eduroam";

// MQTT parameters
const char* mqttServer = "193.147.53.2";
uint16_t mqttPort = 21883;

const char* topic = "/SETR/2023/13/";

// MQTT objects (used for connection and publishing in server)
WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, mqttServer, mqttPort);
Adafruit_MQTT_Publish channel = Adafruit_MQTT_Publish(&mqtt, topic);

String receivedBuff;
const char send = 'c';  // (c)onnected

// Published messages in MQTT
const char* init_circuit = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"START_LAP\"\n}";
const char* end_circuit = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"END_LAP\",\n\t\"time\": ";
const char* detect_dist = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"OBSTACLE_DETECTED\",\n\t\"distance\": ";
const char* lost_circuit = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"LINE_LOST\"\n}";
const char* ping_msg = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"PING\",\n\t\"time\": ";
const char* init_search = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"INIT_LINE_SEARCH\"\n}";
const char* end_search = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"STOP_LINE_SEARCH\"\n}";
const char* line_found = "\n{\n\t\"team_name\": \"LosCopilotos\",\n\t\"id\": \"13\",\n\t\"action\": \"LINE_FOUND\"\n}";

// Variables related to time calculations:
unsigned long init_time = 0;  // Timestamp from begginning of communication
unsigned long check_time = 0; // Used for keeping track of ping intervals (4s)
unsigned long current_time = 0; // Used for keeping track of ping intervals (4s)
unsigned long ping_time = 0;  // Results from substracting current_time from check_time

char received = '\0';

void reconnectMQTT(){
  if(!mqtt.connected()){
    mqtt.connect();
  }
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(10);

  // -------- WiFi connection --------
  WiFi.disconnect(true); 

  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  // -------- MQTT connection --------
  mqtt.connect();
  if (mqtt.connected()){
    // Notify Arduino of succesfull WiFi and MQTT connection
    Serial2.println(send); 
  }

  // Initialize variable
  current_time = millis();
}

void loop() {
  // Reconnect to MQTT if disconnected
  reconnectMQTT();

  // Calculate ping_time (must be aprox. 4s)
  check_time = millis();
  ping_time = (check_time - current_time);

  // Check if there is data available on Serial2
  if (Serial2.available() > 0) {
    received = Serial2.read();

    // Check if the received character is valid (not -1)
    if (received != -1) {
      receivedBuff += received;

      // Starting circuit ('s')
      if (received == 's') {
        receivedBuff = "";

        channel.publish(init_circuit);

        // Initialize variables
        init_time = millis();
        current_time = millis();
        ping_time = 0;
      }

      // Lost circuit ('l')
      if (received == 'l') {
        receivedBuff = "";

        channel.publish(lost_circuit);
        channel.publish(init_search);
      }

      // Found line ('f')
      if (received == 'f') {
        receivedBuff = "";

        channel.publish(line_found);
        channel.publish(end_search);
      }

      // End circuit. Received message is a char containning distance to obstacle
      if (received >= '0' && received <= '9') {
        receivedBuff = "";

        char obstacle[100];
        // Concatenate number (distance) to string and publish
        snprintf(obstacle, sizeof(obstacle), "%s%c\n}\n", detect_dist, received);
        channel.publish(obstacle);

        // Calculate total time
        int end_time = millis();
        int total_time = end_time - init_time;

        char final[100];
        // Concatenate number (time) to string and publish
        snprintf(final, sizeof(final), "%s%d\n}\n", end_circuit, total_time);
        channel.publish(final);

        // End program
        while(1);
      }
    }
  }

  // Check value of ping_time even if no messages are been received
  if (ping_time <= 4005 && ping_time >= 3995) {
    // Update value
    current_time = millis();

    // Calculate total time
    int total_ping_time = current_time - init_time;
    
    char ping[100];
    // Concatenate number (time) to string and publish
    snprintf(ping, sizeof(ping), "%s%d\n}\n", ping_msg, total_ping_time);
    channel.publish(ping);
  }

  received = '\0';
}
