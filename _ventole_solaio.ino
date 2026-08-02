#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// --- COSTANTI LOGICA RELÈ (HIGH = ACCESO, LOW = SPENTO) ---
const int RELE_ACCESO = HIGH;
const int RELE_SPENTO = LOW;

// Credenziali per l'Access Point dell'ESP32
const char *ssid     = "Ventole_solaio";
const char *password = "123456789*"; // Password (minimo 8 caratteri)

WebServer server(80);

#define DHTPIN 4       // Pin GPIO sensore DHT
#define DHTTYPE DHT11  // Cambiare DHT11/DHT22 in funzione del modello utilizzato
DHT dht(DHTPIN, DHTTYPE);

// Configurazione Pin per il Singolo Carico
const int relePin = 23; // Carico 1 (Termostato)

float temperatura = 0.0;
float sogliaAutomatica = 29.0; 
const float isteresi = 0.5;    
String modalita = "AUTO";      // AUTO o MANUALE

// Temporizzazioni sensore
unsigned long precedenteMillisSensore = 0;
const long intervalloSensore = 2000;

// --- GESTIONE TIMER SICUREZZA ---
// Per ripristinare i tempi reali, scommenta le due righe sotto e commenta quelle da 5s/2s
 const unsigned long MAX_TEMPO_ACCESO = 2 * 60 * 60 * 1000UL; // 2 ore
 const unsigned long TEMPO_PAUSA       = 15 * 60 * 1000UL;     // 15 minuti
//const unsigned long MAX_TEMPO_ACCESO = 10000; // 5 secondi solo per test
//const unsigned long TEMPO_PAUSA       = 3000; // 2 secondi solo per test

unsigned long inizioAccensione = 0;
unsigned long inizioPausa = 0;
bool inPausaForzata = false;
bool statoPrecedenteRele = RELE_SPENTO;

void gestisciTimerSicurezza() {
  unsigned long correnteMillis = millis();
  bool statoAttuale = digitalRead(relePin); // HIGH = ACCESO

  // Rileva la transizione da SPENTO ad ACCESO
  if (statoPrecedenteRele == RELE_SPENTO && statoAttuale == RELE_ACCESO) {
    inizioAccensione = correnteMillis;
  }
  statoPrecedenteRele = statoAttuale;

  // Gestione Pausa Forzata
  if (inPausaForzata) {
    digitalWrite(relePin, RELE_SPENTO);
    if (correnteMillis - inizioPausa >= TEMPO_PAUSA) {
      inPausaForzata = false;
    }
    return;
  }

  // Spegne il carico se supera il tempo massimo di accensione
  if (statoAttuale == RELE_ACCESO) {
    if (correnteMillis - inizioAccensione >= MAX_TEMPO_ACCESO) {
      inPausaForzata = true;
      inizioPausa = correnteMillis;
      digitalWrite(relePin, RELE_SPENTO);
    }
  }
}

void leggiSensore() {
  unsigned long correnteMillis = millis();
  
  if (correnteMillis - precedenteMillisSensore >= intervalloSensore) {
    precedenteMillisSensore = correnteMillis;
    float t = dht.readTemperature();
    
    if (!isnan(t)) {
      temperatura = t;
    }
    
    if (modalita == "AUTO" && !inPausaForzata) {
      if (temperatura >= sogliaAutomatica) {
        digitalWrite(relePin, RELE_ACCESO);
      } else if (temperatura < (sogliaAutomatica - isteresi)) {
        digitalWrite(relePin, RELE_SPENTO);
      }
    }
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" charset=\"UTF-8\">";
  html += "<meta http-equiv='refresh' content='5'>";
  
  html += "<style>body { text-align: center; font-family: sans-serif; margin-top: 20px; background-color: #f4f4f9; color: #333; }";
  html += ".card { background: white; padding: 20px; max-width: 440px; margin: 15px auto; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += "a { display: inline-block; padding: 10px 18px; margin: 3px; font-size: 15px; text-decoration: none; color: white; border-radius: 5px; font-weight: bold; }";
  html += ".btn-on { background-color: #4CAF50; } .btn-off { background-color: #f44336; }";
  html += ".btn-mode { background-color: #008CBA; width: 80%; }";
  html += ".btn-soglia { background-color: #ff9800; width: 35px; font-size: 18px; padding: 4px 10px; }";
  html += ".alert { color: #d9534f; font-weight: bold; background: #f8d7da; padding: 10px; border-radius: 5px; border: 1px solid #f5c6cb; font-size: 14px; text-align: left; }";
  html += "h2 { color: #555; margin-bottom: 5px; margin-top: 5px; } .temp-val { font-size: 38px; color: #ff5722; font-weight: bold; margin: 5px 0; }";
  html += ".load-row { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #eee; padding: 12px 0; }";
  html += ".load-title { font-weight: bold; text-align: left; }</style></head>";

  html += "<body><h1>Controllo aerazione solaio</h1>";

  // Avviso Pausa di Sicurezza
  if (inPausaForzata) {
    html += "<div class='card alert'><b>PAUSA DI SICUREZZA ATTIVA:</b><br>";
    unsigned long secRimanenti = (TEMPO_PAUSA - (millis() - inizioPausa)) / 1000;
    html += "&bull; <b>Carico 1</b>: " + String(secRimanenti / 60) + "m " + String(secRimanenti % 60) + "s rimanenti<br></div>";
  }

  // Card 1: Sensore & Termostato
  html += "<div class='card'>";
  html += "<h2>Temperatura attuale</h2>";
  html += "<p class='temp-val'>" + String(temperatura, 1) + " &deg;C</p>";
  html += "<p>Soglia attiv. <b>" + String(sogliaAutomatica, 1) + " &deg;C</b> ";
  html += "<a class=\"btn-soglia\" href=\"/sogliaGiu\">-</a>";
  html += "<a class=\"btn-soglia\" href=\"/sogliaSu\">+</a></p>";
  html += "<p>Modalit&agrave;: <b style='color:#008CBA;'>" + modalita + "</b></p>";
  html += "<a class=\"btn-mode\" href=\"/toggleMode\">MANUALE/AUTOMATICO</a>";
  html += "</div>";

  // Card 2: Controllo Carico 1
  html += "<div class='card'>";
  html += "<h2>Stato sistema</h2>";

  html += "<div class='load-row'>";
  html += "<div class='load-title'>Ventole aspirazione<br><small>Stato: ";
  html += (digitalRead(relePin) == RELE_ACCESO ? "<b style='color:green;'>ON</b>" : "<b style='color:red;'>OFF</b>");
  html += "</small></div>";

  html += "<div>";
  if (inPausaForzata) {
    html += "<span style='color:red; font-size:12px; font-weight:bold;'>IN PAUSA</span>";
  } else if (modalita == "AUTO") {
    html += "<span style='color:gray; font-size:12px;'>Gestito in AUTO</span>";
  } else {
    html += "<a class='btn-on' href='/on1'>ON</a>";
    html += "<a class='btn-off' href='/off1'>OFF</a>";
  }
  html += "</div></div>";

  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Inizializzazione Pin Relè (Partenza da SPENTO)
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, RELE_SPENTO);

  dht.begin();
  
  // Avvio dell'Access Point con SSID e Password
  WiFi.softAP(ssid, password);

  // Gestione Rotte Server Web
  server.on("/", handleRoot);

  server.on("/toggleMode", []() {
    modalita = (modalita == "AUTO") ? "MANUALE" : "AUTO";
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/sogliaSu", []() {
    sogliaAutomatica += 0.5;
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/sogliaGiu", []() {
    sogliaAutomatica -= 0.5;
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/on1", []() { 
    if (!inPausaForzata && modalita == "MANUALE") {
      digitalWrite(relePin, RELE_ACCESO);
    }
    server.sendHeader("Location", "/"); server.send(303); 
  });
  
  server.on("/off1", []() { 
    if (!inPausaForzata && modalita == "MANUALE") {
      digitalWrite(relePin, RELE_SPENTO);
    }
    server.sendHeader("Location", "/"); server.send(303); 
  });

  server.begin();
}

void loop() {
  leggiSensore();
  gestisciTimerSicurezza();
  server.handleClient();
}