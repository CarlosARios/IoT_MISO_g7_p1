#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>
#include "DHT.h"

// --- CONFIGURACIÓN DE SENSORES ---
#define DHTTYPE DHT11 
#define dht_dpin 4
DHT dht(dht_dpin, DHTTYPE);

#define LDR_PIN A0 // Pin analógico para la fotorresistencia

// --- CREDENCIALES ---
// (Asegúrate de que secrets.h exista o define aquí tus certificados si los usas)
#include "secrets.h"

const char ssid[] = "******";        // Nombre de tu red
const char pass[] = "******";      // Contraseña de tu red
#define HOSTNAME "c.riosp"              // Tu usuario

// --- CONFIGURACIÓN MQTT ---
const char MQTT_HOST[] = "iotlab.virtual.uniandes.edu.co";
const int MQTT_PORT = 8082;
const char MQTT_USER[] = "c.riosp";
const char MQTT_PASS[] = "202325487";

const char MQTT_SUB_TOPIC[] = HOSTNAME "/";
const char MQTT_PUB_TOPIC1[] = "humedad/bogota/" HOSTNAME;
const char MQTT_PUB_TOPIC2[] = "temperatura/bogota/" HOSTNAME;
const char MQTT_PUB_TOPIC3[] = "luminosidad/bogota/" HOSTNAME;

// --- CLIENTES DE RED ---
BearSSL::WiFiClientSecure net;
PubSubClient client(net);

time_t now;
unsigned long lastMillis = 0;

void mqtt_connect() {
  while (!client.connected()) {
    Serial.print("Time: ");
    Serial.print(ctime(&now));
    Serial.print("MQTT connecting ... ");
    if (client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected.");
    } else {
      Serial.println("Problema con la conexión, revise los valores de las constantes MQTT");
      Serial.print("Código de error = ");
      Serial.println(client.state());
      if ( client.state() == MQTT_CONNECT_UNAUTHORIZED ) {
        ESP.deepSleep(0);
      }
      delay(5000);
    }
  }
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT); // Configurar pin analógico

  Serial.println("\nIniciando conexión WiFi...");
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    if ( WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_WRONG_PASSWORD ) {
      Serial.print("\nError en credenciales WiFi");
      ESP.deepSleep(0);
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" WiFi conectado!");

  // Sincronización de hora
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    now = time(nullptr);
  }

  // Configuración de Seguridad
  net.setInsecure(); 

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(receivedCallback);
  mqtt_connect();
  
  dht.begin();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
      delay(500);
    }
  } else {
    if (!client.connected()) {
      mqtt_connect();
    } else {
      client.loop();
    }
  }

  now = time(nullptr);

  // --- LECTURA DE SENSORES ---
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  // Lectura LDR y conversión a porcentaje
  int valorCrudo = analogRead(LDR_PIN);
  int porcentajeLuz = map(valorCrudo, 0, 1023, 0, 100);

  // --- CREACIÓN DE PAYLOADS JSON ---
  
  // Humedad
  String jsonH = "{\"value\": "+ String(h) + "}";
  char p1[jsonH.length()+1];
  jsonH.toCharArray(p1, jsonH.length()+1);

  // Temperatura
  String jsonT = "{\"value\": "+ String(t) + "}";
  char p2[jsonT.length()+1];
  jsonT.toCharArray(p2, jsonT.length()+1);

  // Luminosidad
  String jsonL = "{\"value\": "+ String(porcentajeLuz) + "}";
  char p3[jsonL.length()+1];
  jsonL.toCharArray(p3, jsonL.length()+1);

  // --- ENVÍO DE DATOS ---
  if ( !isnan(h) && !isnan(t) ) {
    client.publish(MQTT_PUB_TOPIC1, p1, false);
    client.publish(MQTT_PUB_TOPIC2, p2, false);
    client.publish(MQTT_PUB_TOPIC3, p3, false);

    // Impresión en Monitor Serial
    Serial.print("Humedad: "); Serial.println(p1);
    Serial.print("Temp: "); Serial.println(p2);
    Serial.print("Luz (%): "); Serial.println(p3);
    Serial.println("-----------------------");
  } else {
    Serial.println("Error leyendo el sensor DHT11");
  }

  delay(5000); // Esperar 5 segundos
}
