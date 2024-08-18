/*
 Circuit: 
 >> Ethernet shield attached to pins 10, 11, 12, 13 
 >> Analog inputs attached to pins A0 through A5 (optional)
*/ 
//define macros for arduino uno pins
#define TRIG 2
#define ECH 3

//include header files
#include <SPI.h>
#include <Ethernet.h>
#include "secrets.h"
#include "ThingSpeak.h"//always include thingspeak header file after other header files and custom macros



// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192,168,29,183);


unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);
EthernetClient client;

void setup() {
  //define pin mode
  pinMode(TRIG, OUTPUT);
  pinMode(ECH, INPUT);
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Industrial Machine State Monitoring System");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac);
  delay(1000); //give the Ethernet shield a second to initialize:

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
 
  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}


void loop() {
  static float sensor_data[4] = {0};//declare array to store sensor values
  //declare variables for sensor values
  static float voltage;//for voltage given by potentiometer
  static float distance;//for distance given by ultrasonic sensor
  static unsigned int time;//for time to calculate time of flight of ultrsonic pulse
  static float temp;//for temprature given by LM35

  //calculate voltage given by potentiometer reading
  voltage = float(analogRead(A0)) * 5 / 1024;//voltage reading converted in volt

  //calculate distance measurnment by ultrasound sensor reading
  digitalWrite(TRIG, LOW);//make triggrer pin low
  delayMicroseconds(2);//delay for 2 microseconds to ensure trigger pin is low
  digitalWrite(TRIG, HIGH);//make trigger pin hign
  delayMicroseconds(10);//keep high for 10 microseconds to send ultrasound pulses
  digitalWrite(TRIG, LOW);//make trigger pin low
  //use pulseIn function to get time taken in receiving eco in microseconds
  time = pulseIn(ECH, HIGH);//store time of flight. value is in micro seconds
  //speed of sound in air 0.033 cm / microsecond
  //accounted to and fro distance travelled by ultrasound pulse
  distance = 0.033 * (float)time / 2;//calculate distance in cm

  //calculate temprature measurnment by LM35 sensor reading
  temp = analogRead(A3);//read sensor output from pin value in range 0 to 1023
  //convert in to milli volt value, 0 to 5000 mili volt in range 0 to 1023 values
  temp = temp * 5000 / 1024;
  //convert into *centigrade, using scaling factor 10mv/*c
  temp = temp / 10;

  //store measured values by sensors in array
  sensor_data[0] = voltage;
  sensor_data[1] = distance;
  sensor_data[2] = analogRead(A2);//LDR sensor reading
  sensor_data[3] = temp;

  ThingSpeak.setField(1, voltage);   // Send voltage to Field 1
  ThingSpeak.setField(2, distance);  // Send distance to Field 2
  ThingSpeak.setField(3, sensor_data[2]); // Send LDR reading to Field 3
  ThingSpeak.setField(4, temp);      // Send temperature to Field 4
  
  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an HTTP request ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {    //Need to connect Host at 127.0.0.1:2080
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the HTTP request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard HTTP response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");         // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          
          // opload the value of each sensor
          for (int analogChannel = 1; analogChannel < 5; analogChannel++) {
            float sensorReading = 0;
            sensorReading = sensor_data[analogChannel - 1];//assign each sensor reading one by one from array

            //print sensor reading on serial terminal
            Serial.print("sensorReading ");
            Serial.println(analogChannel);
            Serial.println(sensorReading);

            //upload sensor readings to thing speak
            ThingSpeak.setField(analogChannel, sensorReading);
            ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

            delay(20000);//delay for ThingSpeak to take value and wait till receive next

            //Print analog values to host at 127.0.0.1.2080
            client.print("analog input ");  
            client.print(analogChannel);  
            client.print(" is ");
            client.print(sensorReading);
            client.println("<br />");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}
