#include <WiFi.h>          // Librería para conectarnos a WiFi
#include <HTTPClient.h>    // Librería para hacer peticiones HTTP
#include "config.h"        // Nuestro archivo de configuración con WiFi y API key

// Pines
#define LED_BUILTIN 2      // LED integrado en el ESP32
#define POT_PIN 34         // Pin analógico para el potenciómetro

void setup() {
  Serial.begin(115200);    // Inicia el monitor serial para ver mensajes
  pinMode(LED_BUILTIN, OUTPUT);   // Configura el LED como salida
  connectWiFi();           // Llama a la función que conecta al WiFi
}

void loop() {
  int potValue = analogRead(POT_PIN); // Lectura del potenciómetro
  Serial.println("Potenciómetro: " + String(potValue));

  if (potValue > 3000) {
    String prompt = "Dame una frase motivadora corta.";
    String respuesta = getRespuestaIA(prompt);

    Serial.println("Respuesta IA:");
    Serial.println(respuesta);

    // Indicador con LED
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
  }

  delay(2000);
}

void connectWiFi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Conectado a WiFi!");
}

String getRespuestaIA(String prompt) {
  HTTPClient http;
  String endpoint = API_ENDPOINT;

  http.begin(endpoint);
  http.addHeader("Content-Type", "application/json");

  // Cuerpo del mensaje
  String body = "{\"contents\":[{\"parts\":[{\"text\":\"" + prompt + "\"}]}]}";

  int httpResponseCode = http.POST(body);

  if (httpResponseCode == 200) {
    String response = http.getString();

    // Buscar el contenido real dentro del JSON
    int start = response.indexOf("\"text\":\"");
    int end = response.indexOf("\"", start + 8);

    if (start != -1 && end != -1) {
      String texto = response.substring(start + 8, end);
      return texto;
    } else {
      return "No se pudo interpretar la respuesta.";
    }

  } else {
    return "Error: " + String(httpResponseCode);
  }
}
