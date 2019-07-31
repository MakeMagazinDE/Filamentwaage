//-------------------------------------------------------------------------------------
// Robert Kränzlein November 2018
// Hardware: HX711-Modul, 1kg-Wägezelle, I2C-OLED-Display 128x32 mit SSD1306-Prozessor
// Ledunia (ESP8266 IoT-Board)
// V1.2 - Pushover, 3 Messung mit Mittelwert, Tare-Zählerrücksetzung
//-------------------------------------------------------------------------------------


const char* mqtt_server = "192.168.178.xx";   //IP-Adresse des MQTT-Server im eigenen Netzwerk
const char* ssid = "ssidname";                //Name des eigenen WLAN
const char* password = "passwort";            //WPA-Key des eigenen WLAN
#define APIKEY "hierIFTTTKeyeinsetzen"        //IFTTT-Key von Seite https://ifttt.com/services/maker/settings
#define IFTTTEVENT "filament"                 //Ereignis-Name bei der Erzeugung des Aplets
#define MQTTUSER "mqttuser"		                //hier Benutzername für MQTT-Server einsetzen
#define MQTTPW "mqttpw"			                  //hier Passwort für MQTT-Server einsetzen
const float Calibration = 404.0;              //Ergebnis des Arduino-Programms 'Calibrate' aus den Beispielen der Bibliothek HX711_ADC
const int cyclet = 5000;                      //Häufigkeit der Übertagung an MQTT in ms 5000 = Messung alle 5s
#define Run0 5                                //wieoft die Warung unterdrücken zum Beginn und nach Tare?
#define Warns 25                              //Warnschwelle - unter wieviel Gramm wird eine Warnung ausgegeben?
#define DOUT 13                               //Pins für HX711
#define SCK 15
#define SDA 2                                 //Pins für OLED-Display
#define SCL 14
#define Taster 0                              //Pin für Tare-Taster
#include <HX711_ADC.h>                        //Einbinden der verwendeten Bibliotheken
#include <ESP8266WiFi.h>                      
#include <WiFiClient.h>     
#include <WiFiClientSecure.h>                  
#include <PubSubClient.h>
#include <IFTTTMaker.h>
#include <Wire.h> 
#include <SSD1306Wire.h>
#include <ArduinoJson.h>                      //wird von IFTTTMaker benötigt. Es funktionieren NICHT die neuesten Beta-Version 6.xx.x . Version 5.13.2 funktioniert
#include "font24.h"                           //Einbinden des Schriftsatzes DejaVu_Sans_32

WiFiClient iclient;                           //Internetverbindung anlegen
WiFiClientSecure sclient;
PubSubClient mclient(iclient);                //MQTT-Verbingun
IFTTTMaker wbhook(APIKEY, sclient);
HX711_ADC LoadCell(DOUT, SCK);                //HX711 AD-Wandler definieren (dout pin, sck pin)
SSD1306Wire  display(0x3c, SDA, SCL);         //OLED-Display definieren über Wire-Library

long t;                                       //Variable für Systemzeit
int runup = Run0;                             //Anzahl der ersten Durchläufe ohne Filamentwarnung                                    

void setup() {                                //Setuproutine bei Programmstart
  Serial.begin(115200);
  delay(1000);
  Serial.println("Startup...");
  WiFi.mode(WIFI_STA);                        //WLan-Verbindung herstellen
  WiFi.begin(ssid, password);
  connect();
  mclient.setServer(mqtt_server, 1883);       //Verbindung zum MQTT-Broker
  pinMode(Taster,INPUT_PULLUP);               //Tare-Taster als Eingang definieren
  
  LoadCell.begin();                           //Initialisierung der Wägezellt
  long stabilisingtime = 2000;                //2 Sekunden warten damit Nullpunkt gemessen werden kann
  LoadCell.start(stabilisingtime);            //Wägezelle starten
  LoadCell.setCalFactor(Calibration);         //Kalibrierung an HX711 übertragen
  delay(2000);
  Serial.println("Startup + tare is complete");

  display.init();                             //OLED initialisieren
  display.flipScreenVertically();             //Bildschirmdarstellung um 180° drehen (bei Bedarf)
  display.setFont(ArialMT_Plain_16);          //Fontgröße 16 (eingebaut)
  display.drawString(0, 10, "Setup complete");
  Serial.println("Display ready");
  display.drawString(0, 34, WiFi.localIP().toString());
  display.display();                          //Ausgabe an Display senden
  delay(5000);                                //5 Sekunden warten        
  display.setTextAlignment(TEXT_ALIGN_RIGHT); //Text ab jetzt rechtsbündig, die Koordinaten geben dann auch das rechte Ende des Textes an
}

void connect() {                              //Routine connect wird solange wiederholt bis Internet-Verbinung besteht
  Serial.println("First connection");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  WiFi.begin(ssid, password);
  }
  Serial.println(WiFi.localIP());
  
}

void reconnect() {                            //Routine baut Verbindung zum MQTT-Server auf
  while (!mclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "Filamentwaage";
      if (mclient.connect(clientId.c_str(), mqttuser, mqttpw)) {
      Serial.println("connected");
      display.setFont(ArialMT_Plain_16);
      display.drawString(0, 34, "MQTT connected");
      display.display(); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(mclient.state());
      Serial.println(" try again in 5 seconds");
      display.setFont(ArialMT_Plain_16);
      display.drawString(0, 34, "MQTT failed");
      display.display();      
      delay(5000);                            //Ebenfalls 5 Sekunden Wartezeit
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
 } 


void loop() {                               //Hauptprogramm
  if (!mclient.connected()) {               //Ist Internetverbindung noch da?
    reconnect();                            //falls nein -> neu verbinden
  }                                         //falls ja -> 
  mclient.loop();                            
  LoadCell.update();                        //Neue Messung der Wägezelle holen

 if (millis() > t + cyclet) {               //Wieviel Zeit ist seit der letzten Messung vergangen?
    float i1 = LoadCell.getData();          //Wägezelle auslesen - erste Messung
    delay(250);
    float i2 = LoadCell.getData();          //Wägezelle auslesen - zweite Messung
    delay(250);
    float i3 = LoadCell.getData();          //Wägezelle auslesen - dritte Messung
    float i=(i1+i2+i3)/3;                   //Mittelwert bilden
    int j = (int)i;                         //Float in Integer umwandeln
     Serial.print("Load_cell output val: ");
     Serial.println(i);
    t = millis();                           //neue Zeit der letzten Messung speichern
                                            //auf Display anzeigen
    String wicht = String(j);               //Zahl in String umwandeln
    String wichtg = String(wicht + " g");   //Leerzeichen und g anhängen
    display.clear();                        //Display löschen
    display.setFont(DejaVu_Sans_32);        //großen Font verwenden aus font24.h
    display.drawString(128, 26, wichtg);    //Textposition rechts unten
    display.display();                      //Darstellung ausgeben
                                            //Per MQTT-Daten versenden
    char buffer[10];                        //10 Zeichen reservieren
    sprintf(buffer, "%li", j);              //Integer-Wert in Chars umwandeln
    mclient.publish("/ledunia/prusa/status", buffer);   //und an MQTT senden
                                            
    if (j<Warns){
      if (runup<=0) {     
        Serial.println("Filamentwarnung");
        if(wbhook.triggerEvent(IFTTTEVENT, wicht)) {
          Serial.println("Successfully sent");
          runup = Run0;
        } else {
          Serial.println("Failed!");    
        }
      } else {
        runup = runup-1;
        Serial.println(runup);
      }
    }

    if (digitalRead(Taster)==LOW) {          //Wurde der Taster für Tare gedrückt?
      LoadCell.tareNoDelay();                //Wägezelle tarieren
      Serial.println("Taster Tare");
      display.clear();
      display.drawString(100, 26, "Tare");
      display.display();
      delay(1000);
      if (LoadCell.getTareStatus() == true) { //Ist Tare abgeschlossen? 
        Serial.println("Tare complete");      //Wenn ja Ausgabe in Serielle Konsole
        display.clear();
        display.display();
      runup = Run0;                       //Zähler für keine Warnung zurücksetzen
      }
    }  
  }

  if (Serial.available() > 0) {           //Wenn über das Terminal etwas gesendet wurde Eingabe verarbeiten
    char inByte = Serial.read();          //Eingabe abholen
    if (inByte == 't') {                  //Eingabe t löst Tare-Funktion aus
      LoadCell.tareNoDelay();             //Wägezelle tarieren
      Serial.println("Serial Tare");
      display.clear();
      display.drawString(100, 26, "Tare");
      display.display();
      delay(1000);
      if (LoadCell.getTareStatus() == true) {  //Ist Tare abgeschlossen? 
        Serial.println("Tare complete");      //Wenn ja Ausgabe in serielle Konsole
        display.clear();
        display.display();
      runup = Run0;                       //Zähler für keine Warnung zurücksetzen
      }
    }
  }
 

}
