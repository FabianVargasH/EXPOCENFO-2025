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
  // Leer el valor del potenciómetro (0 - 4095 en ESP32)
  int potValue = analogRead(POT_PIN);
  Serial.println("Potenciómetro: " + String(potValue));

  // Si el potenciómetro supera un umbral (ej: lo giraste fuerte)
  if (potValue > 3000) {
    String prompt = "Dame una frase motivadora corta.";
    String respuesta = getRespuestaIA(prompt);

    Serial.println("Respuesta IA: ");
    Serial.println(respuesta);

    // Indicación visual con el LED
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
  }

  delay(2000); // Espera 2 segundos antes de leer de nuevo
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
  http.begin(endpoint);                     // Inicia la conexión al endpoint de Gemini
  http.addHeader("Content-Type", "application/json");

  // Cuerpo de la petición con el prompt
  String body = "{\"contents\":[{\"parts\":[{\"text\":\"" + prompt + "\"}]}]}";

  int httpResponseCode = http.POST(body);   // Envía la petición POST

  if (httpResponseCode == 200) {
    String response = http.getString();     // Respuesta de la API
    return response;
  } else {
    return "Error: " + String(httpResponseCode);
  }
}
