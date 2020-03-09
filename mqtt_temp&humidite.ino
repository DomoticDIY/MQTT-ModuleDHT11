
/*
 ESP8266 MQTT - Relevé de température et humidité via DHT11
 Création Dominique PAUL.
 Dépot Github : https://github.com/DomoticDIY/MQTT-ModuleDHT11
  Chaine YouTube du Tuto Vidéo : https://www.youtube.com/c/DomoticDIY
 
 Bibliothéques nécessaires :
  - pubsubclient : https://github.com/knolleary/pubsubclient
  - ArduinoJson : https://github.com/bblanchon/ArduinoJson
 Télécharger les bibliothèques, puis dans IDE : Faire Croquis / inclure une bibliothéque / ajouter la bibliothèque ZIP.
 Puis dans IDE : Faire Croquis / inclure une bibliothéque / Gérer les bibliothèques, et ajouter :
  - DHT Sensor Library by AdaFruit
  - AdaFruit Unified sensor by AdaFruit
 
 Dans le gestionnaire de bibliothéque, charger le module ESP8266Wifi.
 Installer le gestionnaire de carte ESP8266 version 2.5.0 
 Si besoin : URL à ajouter pour le Bord manager : http://arduino.esp8266.com/stable/package_esp8266com_index.json
 Adaptation pour reconnaissance dans Domoticz :
 Dans le fichier PubSubClient.h : La valeur du paramètre doit être augmentée à 512 octets. Cette définition se trouve à la ligne 26 du fichier.
 Sinon cela ne fonctionne pas avec Domoticz
 Pour prise en compte du matériel :
 Installer si besoin le Driver USB CH340G : https://wiki.wemos.cc/downloads
 dans Outils -> Type de carte : generic ESP8266 module
  Flash mode 'QIO' (régle générale, suivant votre ESP, si cela ne fonctionne pas, tester un autre mode.
  Flash size : 1M (no SPIFFS)
  Port : Le port COM de votre ESP vu par windows dans le gestionnaire de périphériques.
*/

// Inclure les librairies.
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>


// Déclaration des constantes, données à adapter à votre réseau.
// ------------------------------------------------------------
const char* ssid = "_MON_SSID_";                // SSID du réseau Wifi
const char* password = "_MOT_DE_PASSE_WIFI_";   // Mot de passe du réseau Wifi.
const char* mqtt_server = "_IP_DU_BROKER_";     // Adresse IP ou DNS du Broker.
const int mqtt_port = 1883;                     // Port du Brocker MQTT
const char* mqtt_login = "_LOGIN_";             // Login de connexion à MQTT.
const char* mqtt_password = "_PASSWORD_";       // Mot de passe de connexion à MQTT.
// ------------------------------------------------------------
// Variables de configuration :
#define DHTTYPE DHT11                                 // DHT 11 // DHT 22 (AM2302) // DHT 21 (AM2301)
#define DHTPIN  2
char* topicIn = "domoticz/out";                       // Nom du topic envoyé par Domoticz
char* topicOut = "domoticz/in";                       // Nom du topic écouté par Domoticz
float valHum = 0.0;                                    // Variables contenant la valeur de l'humidité.
float valTemp = 0.0;                                    // Variables contenant la valeur de température.
float valHum_T, valTemp_T;                            // Valeurs de relevé temporaires.
#define tempsPause 120                                // Nbre de secondes de pause (3600 = 1H00)
// ------------------------------------------------------------
// Variables et constantes utilisateur :
String nomModule = "Température & Humidité";          // Nom usuel de ce module. Sera visible uniquement dans les Log Domoticz.
int idxDevice = 29;                                   // Index du Device à actionner.
// ------------------------------------------------------------


WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE, 11);
char buf[20];


// SETUP
// *****
void setup() {
  Serial.begin(115200);                       // On initialise la vitesse de transmission de la console.
  dht.begin();                                // On initialise le DHT sensor
  setup_wifi();                               // Connexion au Wifi
  client.setServer(mqtt_server, mqtt_port);   // On défini la connexion MQTT
}

// BOUCLE DE TRAVAIL
// *****************
void loop() {
  if (!client.connected()) {
    Serial.println("MQTT déconnecté, on reconnecte !");
    reconnect();
  } else {
    // On interroge la sonde de Température / Humidité.
    getTempHum();
    // Envoi de la données via JSON et MQTT
    SendData();
    // On met le système en pause pour un temps défini
    delay(tempsPause * 1000);
  }
}


// CONNEXION WIFI
// **************
void setup_wifi() {
  // Connexion au réseau Wifi
  delay(10);
  Serial.println();
  Serial.print("Connection au réseau : ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    // Tant que l'on est pas connecté, on boucle.
    delay(500);
    Serial.print(".");
  }
  // Initialise la séquence Random
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connecté");
  Serial.print("Addresse IP : ");
  Serial.println(WiFi.localIP());
}

// CONNEXION MQTT
// **************
void reconnect() {
  // Boucle jusqu'à la connexion MQTT
  while (!client.connected()) {
    Serial.print("Tentative de connexion MQTT...");
    // Création d'un ID client aléatoire
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Tentative de connexion
    if (client.connect(clientId.c_str(), mqtt_login, mqtt_password)) {
      Serial.println("connecté");
      
      // Connexion effectuée, publication d'un message...
      String message = "Connexion MQTT de "+ nomModule + " réussi sous référence technique : " + clientId + ".";
      // String message = "Connexion MQTT de "+ nomModule + " réussi.";
      StaticJsonBuffer<256> jsonBuffer;
      // Parse l'objet root
      JsonObject &root = jsonBuffer.createObject();
      // On renseigne les variables.
      root["command"] = "addlogmessage";
      root["message"] = message;
      
      // On sérialise la variable JSON
      String messageOut;
      if (root.printTo(messageOut) == 0) {
        Serial.println("Erreur lors de la création du message de connexion pour Domoticz");
      } else  {
        // Convertion du message en Char pour envoi dans les Log Domoticz.
        char messageChar[messageOut.length()+1];
        messageOut.toCharArray(messageChar,messageOut.length()+1);
        client.publish(topicOut, messageChar);
      }
        
      // On souscrit (écoute)
      client.subscribe("#");
    } else {
      Serial.print("Erreur, rc=");
      Serial.print(client.state());
      Serial.println(" prochaine tentative dans 5s");
      // Pause de 5 secondes
      delay(5000);
    }
  }
}

// RELEVE DE TEMPERATURE ET HUMIDITE.
// *********************************
void getTempHum() {
  // On relévé la température.
  // ------------------------
  valTemp_T = dht.readTemperature();        // Lecture de la température : (Celcius par défaut, "dht.readTemperature(true)" = Fahrenheit)
  // On vérifie que le relevé est valide.
  if (isnan(valTemp_T)) {
  // La valeur retournée n'es pas valide (isnan = is Not A Number).
    Serial.println("Erreur lors du relevé de température, on concerve l'ancienne valeur !");
  } else {
  Serial.print("Valeur de température relevée : ");
  Serial.println(valTemp_T);
  valTemp = valTemp_T;
  }
  
  // On reléve l'humidité.
  // --------------------
  valHum_T = dht.readHumidity();            // Lecture de l'humidité (en %)
  if (isnan(valHum_T)) {
  // La valeur retournée n'es pas valide (isnan = is Not A Number).
    Serial.println("Erreur lors du relevé de l'humidité, on concerve l'ancienne valeur !");
  } else {
    Serial.print("Valeur d'humidité relevée : ");
    Serial.println(valHum_T);
    valHum = valHum_T;
  }
}

void SendData () {
  // Création et Envoi de la donnée JSON.
  StaticJsonBuffer<256> jsonBuffer;
  // Parse l'objet root
  JsonObject &root = jsonBuffer.createObject();
  // On renseigne les variables.
  root["type"]    = "command";
  root["param"]   = "udevice";
  root["idx"]     = idxDevice;
  root["nvalue"]  = 0;
  root["svalue"]  = String(valTemp)+";"+String(valHum)+";0"; // txtValRetour;
      
  // On sérialise la variable JSON
  String messageOut;
  if (root.printTo(messageOut) == 0) {
    Serial.println("Erreur lors de la création du message de connexion pour Domoticz");
  } else  {
    // Convertion du message en Char pour envoi dans les Log Domoticz.
    char messageChar[messageOut.length()+1];
    messageOut.toCharArray(messageChar,messageOut.length()+1);
    client.publish(topicOut, messageChar);
    // Pause de 5 secondes
    delay(5000);
    Serial.println("Message envoyé à Domoticz");
  }
}
