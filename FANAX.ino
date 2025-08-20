#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"  // WIFI_SSID, WIFI_PASS, GEMINI_MODEL, API_KEY

// ====== Opciones rápidas ======
#define USE_DHT 0            // 0 = sin DHT, 1 = con DHT
#define PRINT_DIAGNOSTICS 1  // 1 = ver logs útiles; 0 = consola limpia

#if USE_DHT
  #include <DHT.h>
  #define DHTPIN   4
  #define DHTTYPE  DHT11
  DHT dht(DHTPIN, DHTTYPE);
  float lastT = NAN, lastH = NAN;
  unsigned long lastDHTms = 0;
  const unsigned long DHT_PERIOD_MS = 2500;
#else
  float lastT = NAN, lastH = NAN; // placeholders
#endif

// ================= Pines =================
#define LED_BUILTIN 2
#define LIGHT_AO 34        // ADC1_CH6
#define MIC_AO   35        // ADC1_CH7

// OLED SSD1306 I2C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================ Calibración / Filtros ================
struct Calib { float mid=0.5, span=0.5; };
Calib calibL = {0.5,0.5};

// Luz
float emaL = 0.5f;
const float ALPHA_L = 0.20f;
bool useRawLux = false;

// Adaptación dinámica de luz (para cuando el sensor satura)
float l_runMin = 1.0f, l_runMax = 0.0f;
unsigned long lastLAdaptMs = 0;
const float L_DECAY_PER_S = 0.06f;    // cuán rápido “olvida” extremos (~6%/s)

// ===== Ruido (envolvente con ataque/release) =====
float baseN_v = 0.5f;     // línea base lenta (promedio)
const float ALPHA_BASE_N = 0.002f; // base MUY lenta

// Envelope con ataque rápido / release lento:
// si dev > envN usamos attack (sube rápido), si no, release (baja lento).
float envN = 0.0f;
const float ATTACK  = 0.20f;  // 0..1 (más alto = sube más rápido)
const float RELEASE = 0.02f;  // 0..1 (más alto = baja más rápido)

// Auto-ganancia mic
float micMult = 80.0f;            // << más sensible de entrada
unsigned long lastAutoGain = 0;
float envMaxWindow = 0.0f;
const unsigned long AUTO_GAIN_WINDOW_MS = 3000;  // evalúa cada 3 s
const float ENV_PEAK_MIN = 0.0012f;              // si en 3 s el pico < esto, sube ganancia
const float MIC_MULT_STEP = 30.0f;               // incremento de ganancia
const float MIC_MULT_MAX  = 200.0f;              // tope

// ===== Detección de ruido alto sostenido =====
float noiseEMA = 0.0f;                 // promedio suave de ruido 0..1
const float ALPHA_NOISE_EMA = 0.10f;   // 0..1 (≈ 1–2 s)
unsigned long noiseHighSince = 0;
const unsigned long NOISE_SUSTAIN_MS = 6000;  // debe durar 6 s para avisar

// ===== Umbrales =====
const float LUX_OK    = 0.60f;
const float LUX_LOW   = 0.35f;
// Ajustados: silencio/normal ≈5%, alto ≥25%
const float NOISE_OK  = 0.05f;
const float NOISE_HI  = 0.25f;

// ================ FSM =================
enum State { IDLE, FOCUS, BREAK_S };
State state = IDLE, prevState = IDLE;
unsigned long stateStartMs = 0;

// ================ Rate limit / debug / OLED =================
unsigned long lastLLMms = 0;
unsigned long llmBackoffMs = 0;       // backoff por 429
const unsigned long LLM_MIN_INTERVAL_MS = 30000; // 30s
const unsigned long PRINT_INTERVAL_MS = 10000;   // logs cada 10 s
unsigned long lastPrint = 0;
const unsigned long OLED_MSG_MS = 6000;          // consejo ~6 s
unsigned long lastAdviceMs = 0;

// ================ Prototipos =================
void connectWiFi();
String getRespuestaIA(const String& prompt);
String offlineSuggestion(State s, int minutes, bool dim, bool noisy, float T, float H);
const char* stateName(State s);
void oledShow(const String& line1, const String& line2);
void oledShowStatus(float lux, float noise, State s);
float clamp01(float x);
void maybeAskLLM(State s, float lux, float noise, bool dim, bool noisy, float T, float H);

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // ADC: 12 bits. Luz a 11 dB (0..3.3V). Mic mejor a 11 dB para rango completo.
  analogReadResolution(12);
  analogSetPinAttenuation(LIGHT_AO, ADC_11db);
  analogSetPinAttenuation(MIC_AO,   ADC_11db);

  // I2C + OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    if (PRINT_DIAGNOSTICS) Serial.println("OLED no encontrada (0x3C). Continuo sin OLED...");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    oledShow("FANAX", "Inicializando...");
  }

  connectWiFi();

  // ---- Calibración guiada de 3 s ----
  if (PRINT_DIAGNOSTICS) Serial.println("Calibracion luz (3s): tapa 1s, destapa y sube brillo 2s.");
  float minL=1,maxL=0, minN=1,maxN=0;
  unsigned long t0=millis(), now;
  float vL=0, vN=0;

  while ((now=millis())-t0 < 3000) {
    int rawL = analogRead(LIGHT_AO);
    int rawN = analogRead(MIC_AO);
    vL = rawL/4095.0f;
    vN = rawN/4095.0f;
    if (vL<minL) minL=vL; if (vL>maxL) maxL=vL;
    if (vN<minN) minN=vN; if (vN>maxN) maxN=vN;
    delay(10);
  }

  if ((maxL - minL) < 0.02f) {
    useRawLux = true;   // poco rango en calibración → usar crudo + adaptación dinámica
  } else {
    calibL.mid  = (minL+maxL)/2.0f;
    calibL.span = max(0.10f,(maxL-minL)/2.0f);
  }
  // inicializar EMA y ventanas dinámicas
  emaL     = useRawLux ? vL : clamp01((vL - calibL.mid)/(calibL.span*2.0f) + 0.5f);
  l_runMin = minL; l_runMax = maxL;
  baseN_v  = vN;
  envN     = 0.0f;
  lastLAdaptMs = millis();

  stateStartMs = millis();
  if (PRINT_DIAGNOSTICS) {
    Serial.printf("Calib Luz: min=%.3f max=%.3f %s\n", minL, maxL, useRawLux ? "(usar crudo+adapt)" : "(normalizado)");
    Serial.printf("Calib Ruido: min=%.3f max=%.3f (envolvente)\n", minN, maxN);
  }

  oledShow("FANAX listo","Luz & sonido activos");
  lastAdviceMs = millis();
}

// ================= Loop =================
void loop() {
  // Lecturas crudas
  int rawL = analogRead(LIGHT_AO);
  int rawN = analogRead(MIC_AO);
  float vL = rawL/4095.0f;
  float vN = rawN/4095.0f;

  // Autoprotección si el mic está clavado en 0
  if (rawN < 5) vN = baseN_v;

  // -------- Luz: normalización y adaptación dinámica --------
  bool nearTop = (rawL > 3800); // ~92% del ADC
  unsigned long nowMs = millis();
  float dt = (nowMs - lastLAdaptMs) / 1000.0f;
  lastLAdaptMs = nowMs;
  float decay = L_DECAY_PER_S * dt;         // cuánto sube min y baja max por segundo
  l_runMin += decay; if (vL < l_runMin) l_runMin = vL;
  l_runMax -= decay; if (vL > l_runMax) l_runMax = vL;
  float dynRange = l_runMax - l_runMin;
  if (dynRange < 0.05f) { l_runMin = min(l_runMin, vL-0.03f); l_runMax = max(l_runMax, vL+0.03f); dynRange = l_runMax - l_runMin; }

  float luxNorm = clamp01((vL - l_runMin) / dynRange);
  float lux = (useRawLux || nearTop) ?
              luxNorm :
              clamp01((vL - calibL.mid)/(calibL.span*2.0f) + 0.5f);

  if (rawL >= 4000) lux = 1.0f;
  if (rawL <= 100)  lux = 0.0f;

  emaL = ALPHA_L*lux + (1-ALPHA_L)*emaL;

  // -------- Ruido: línea base + envelope con ataque/release --------
  baseN_v = (1-ALPHA_BASE_N)*baseN_v + ALPHA_BASE_N*vN;
  float dev = fabsf(vN - baseN_v);

  // Ataque rápido (sube veloz con picos), release lento (baja despacio)
  if (dev > envN) {
    envN = (1.0f - ATTACK) * envN + ATTACK * dev;     // subir rápido
  } else {
    envN = (1.0f - RELEASE) * envN + RELEASE * dev;   // bajar lento
  }

  // Auto-ganancia más agresiva cada 3 s si no vemos picos
  if (envN > envMaxWindow) envMaxWindow = envN;
  if (millis() - lastAutoGain > AUTO_GAIN_WINDOW_MS) {
    lastAutoGain = millis();
    if (envMaxWindow < ENV_PEAK_MIN && micMult < MIC_MULT_MAX) {
      micMult = min(MIC_MULT_MAX, micMult + MIC_MULT_STEP);
      if (PRINT_DIAGNOSTICS) Serial.printf("[MIC] Subiendo multiplicador a %.1f (peak=%.5f)\n", micMult, envMaxWindow);
    }
    envMaxWindow = 0.0f;
  }

  float noise = clamp01(envN * micMult);

  // Promedio suave de ruido (para “sostenido”)
  noiseEMA = (1.0f - ALPHA_NOISE_EMA) * noiseEMA + ALPHA_NOISE_EMA * noise;
  bool noiseIsHigh = (noiseEMA > NOISE_HI);
  if (noiseIsHigh) {
    if (noiseHighSince == 0) noiseHighSince = millis();
  } else {
    noiseHighSince = 0;
  }
  bool noisySustained = (noiseHighSince != 0) && (millis() - noiseHighSince >= NOISE_SUSTAIN_MS);

  // -------- Contexto --------
  bool dim    = (emaL < LUX_LOW);
  bool bright = (emaL > LUX_OK);
  bool quiet  = (noise < NOISE_OK);

  // Disparadores por evento
  static bool wasDim=false, wasBright=false, wasNoisySust=false;
  if (dim && !wasDim)        { maybeAskLLM(IDLE,  emaL, noiseEMA, dim, false, lastT, lastH); }
  if (bright && !wasBright)  { maybeAskLLM(FOCUS, emaL, noiseEMA, dim, false, lastT, lastH); }
  if (noisySustained && !wasNoisySust) {
    // Ruido alto sostenido > 25% por ≥ 6 s
    maybeAskLLM(BREAK_S, emaL, noiseEMA, dim, true, lastT, lastH);
  }
  wasDim       = dim;
  wasBright    = bright;
  wasNoisySust = noisySustained;

  // FSM
  prevState = state;
  if (bright && quiet)            state = FOCUS;
  else if (dim && noisySustained) state = BREAK_S; // solo si ruido es alto sostenido
  else                            state = IDLE;

  if (state != prevState) {
    stateStartMs = millis();
    maybeAskLLM(state, emaL, noiseEMA, dim, noisySustained, lastT, lastH);
  }

  // LED
  if (state == BREAK_S)           digitalWrite(LED_BUILTIN, ((millis()/800)%2) ? HIGH : LOW);
  else if (state == FOCUS)        digitalWrite(LED_BUILTIN, HIGH);
  else                            digitalWrite(LED_BUILTIN, LOW);

  // OLED: volver a panel cada cierto rato
  if (display.width()!=0 && (millis()-lastAdviceMs > OLED_MSG_MS)) {
    oledShowStatus(emaL, noiseEMA, state);
  }

  // Debug (silenciado si PRINT_DIAGNOSTICS=0)
  if (PRINT_DIAGNOSTICS && (millis()-lastPrint > PRINT_INTERVAL_MS)) {
    lastPrint = millis();
    Serial.print("rawL="); Serial.print(rawL);
    Serial.print(" rawN="); Serial.print(rawN);
    Serial.print(" | lux=");   Serial.print(emaL,3);
    Serial.print(" noiseEMA=");Serial.print(noiseEMA,3);
    Serial.print(" (envN=");   Serial.print(envN,5);
    Serial.print(", mult=");   Serial.print(micMult,1);
    Serial.print(")");
    Serial.print(" | state="); Serial.println(stateName(state));
  }

  delay(40);
}

// ================= Helpers =================
float clamp01(float x){ if (x<0) return 0; if (x>1) return 1; return x; }

void oledShow(const String& line1, const String& line2){
  if (display.width()==0) return;
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(line1);
  display.setTextSize(1);
  String l2=line2; if (l2.length()>22) l2=l2.substring(0,22);
  display.setCursor(0,32);
  display.println(l2);
  display.display();
  lastAdviceMs = millis();
}

void oledShowStatus(float lux, float noise, State s){
  if (display.width()==0) return;
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(stateName(s));
  display.setTextSize(1);
  char l1[32];
  snprintf(l1, sizeof(l1), "L:%d%%  N:%d%%", (int)(lux*100.0f), (int)(noise*100.0f));
  display.setCursor(0,32);
  display.println(l1);

  // barras simples para ver cambios
  int lx = (int)(lux * 120);
  int nx = (int)(noise * 120);
  display.drawRect(0, 44, 120, 6, SSD1306_WHITE);
  display.fillRect(1, 45, max(1,lx), 4, SSD1306_WHITE);
  display.drawRect(0, 54, 120, 6, SSD1306_WHITE);
  display.fillRect(1, 55, max(1,nx), 4, SSD1306_WHITE);

  display.display();
}

const char* stateName(State s){
  switch(s){case IDLE:return "IDLE";case FOCUS:return "FOCUS";case BREAK_S:return "BREAK";}
  return "IDLE";
}

// ================= WiFi / IA =================
void connectWiFi(){
  if (PRINT_DIAGNOSTICS) Serial.println("Conectando a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t=millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-t<15000){ delay(300); if (PRINT_DIAGNOSTICS) Serial.print("."); }
  if (WiFi.status()==WL_CONNECTED){
    if (PRINT_DIAGNOSTICS) Serial.println(String("\n¡Conectado! IP: ")+WiFi.localIP().toString());
  } else {
    if (PRINT_DIAGNOSTICS) Serial.println("\nNo se pudo conectar (seguira intentando).");
  }
}

String offlineSuggestion(State s, int minutes, bool dim, bool noisy, float T, float H){
  if (dim)         return "Luz baja: enciende lámpara para evitar fatiga visual";
  if (noisy)       return "Baja un poco el volumen o usa audífonos";
  if (s==FOCUS)    return "Buen ambiente: descansa 2-3 min cada 25 min";
  if (s==BREAK_S)  return "Pausa breve: estira cuello y hombros";
  return "Ambiente estable: respira y continúa";
}

void maybeAskLLM(State s, float lux, float noise, bool dim, bool noisy, float T, float H){
  if (millis() < llmBackoffMs) return;                 // backoff por 429
  if (millis()-lastLLMms < LLM_MIN_INTERVAL_MS) return;
  lastLLMms = millis();

  String prompt = "Eres un asistente ambiental conciso. Devuelve SOLO UNA frase (<=12 palabras) en español, clara y accionable. ";
  prompt += "Contexto: estado=" + String(stateName(s)) + ", luz=" + String(lux,2) + ", ruido=" + String(noise,2);
#if USE_DHT
  if(!isnan(T)) prompt += ", temp=" + String(T,1) + "C";
  if(!isnan(H)) prompt += ", hum="  + String(H,0) + "%";
#endif
  prompt += ". Reglas: ";
  prompt += "si luz >0.60 sugiere bajar brillo o modo nocturno; ";
  prompt += "si luz <0.35 sugiere encender lámpara; ";
  prompt += "si ruido >0.25 sostenido sugiere bajar volumen o usar audífonos; ";
  prompt += "devuelve solo una frase breve y accionable, sin emojis.";

  String resp = getRespuestaIA(prompt);
  if (resp.startsWith("Error") || resp=="Sin WiFi" || resp=="Respuesta sin texto" || resp=="Error parseando JSON")
    resp = offlineSuggestion(s, 0, dim, noisy, T, H);

  Serial.println("Consejo:");
  Serial.println(resp);
  oledShow(String(stateName(s)), resp);

  digitalWrite(LED_BUILTIN, HIGH); delay(120); digitalWrite(LED_BUILTIN, LOW);
}

// ===== Gemini =====
String getRespuestaIA(const String& prompt){
  if (WiFi.status()!=WL_CONNECTED){ connectWiFi(); if (WiFi.status()!=WL_CONNECTED) return "Sin WiFi"; }
  String baseUrl="https://generativelanguage.googleapis.com/v1beta/models/";
  String url = baseUrl + String(GEMINI_MODEL) + ":generateContent";

  StaticJsonDocument<512> req;
  JsonArray contents=req.createNestedArray("contents");
  JsonObject msg=contents.createNestedObject();
  msg["role"]="user";
  JsonArray parts=msg.createNestedArray("parts");
  parts.createNestedObject()["text"]=prompt;
  String body; serializeJson(req,body);

  { // header key
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.begin(client,url);
    http.addHeader("Content-Type","application/json");
    http.addHeader("x-goog-api-key", API_KEY);
    int code=http.POST(body); String response=http.getString(); http.end();
    if(code==200){
      StaticJsonDocument<4096> doc;
      if(deserializeJson(doc,response)) return "Error parseando JSON";
      JsonArray cand=doc["candidates"];
      if(!cand.isNull() && cand.size()>0){
        JsonArray p=cand[0]["content"]["parts"];
        if(!p.isNull() && p.size()>0 && p[0]["text"].is<const char*>())
          return String(p[0]["text"].as<const char*>());
      }
      return "Respuesta sin texto";
    } else {
      if (code == 429) { llmBackoffMs = millis() + 120000; } // 2 min de enfriamiento
      if (PRINT_DIAGNOSTICS) {
        Serial.println(String("[Gemini][HeaderKey] Error HTTP: ")+code);
        Serial.println(String("[Gemini][HeaderKey] Body: ")+response);
      }
      if(code!=400) return "Error HTTP: "+String(code);
    }
  }
  { // ?key=
    WiFiClientSecure client; client.setInsecure();
    String urlWithKey = url + "?key=" + String(API_KEY);
    HTTPClient http; http.begin(client,urlWithKey);
    http.addHeader("Content-Type","application/json");
    int code=http.POST(body); String response=http.getString(); http.end();
    if(code==200){
      StaticJsonDocument<4096> doc;
      if(deserializeJson(doc,response)) return "Error parseando JSON";
      JsonArray cand=doc["candidates"];
      if(!cand.isNull() && cand.size()>0){
        JsonArray p=cand[0]["content"]["parts"];
        if(!p.isNull() && p.size()>0 && p[0]["text"].is<const char*>())
          return String(p[0]["text"].as<const char*>());
      }
      return "Respuesta sin texto";
    } else {
      if (code == 429) { llmBackoffMs = millis() + 120000; } // 2 min de enfriamiento
      if (PRINT_DIAGNOSTICS) {
        Serial.println(String("[Gemini][QueryKey] Error HTTP: ")+code);
        Serial.println(String("[Gemini][QueryKey] Body: ")+response);
      }
      return "Error HTTP: "+String(code);
    }
  }
}
