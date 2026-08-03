#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <time.h>

// --- COSTANTI LOGICA RELÈ (HIGH = ACCESO, LOW = SPENTO) ---
const int RELE_ACCESO = HIGH;
const int RELE_SPENTO = LOW;

// Credenziali per l'Access Point dell'ESP32
const char *ssid     = "Ventole_solaio";
const char *password = "123456789*"; 

WebServer server(80);

#define DHTPIN 4        
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

const int relePin = 23; 

float temperatura = 0.0;
float sogliaAutomatica = 29.0; 
const float isteresi = 0.5;    
String modalita = "AUTO";      // AUTO o MANUALE

// --- PROGRAMMAZIONE ORARIA ---
bool usaFasciaOraria = true;   // Abilita/Disabilita il controllo orario
int oraInizio = 22;            // Ora inizio (es. 22:00)
int oraFine = 7;               // Ora fine (es. 07:00)
bool oraSincronizzata = false;

// Memorizza lo stato richiesto
bool statoDesiderato = RELE_SPENTO; 

// Temporizzazioni sensore
unsigned long precedenteMillisSensore = 0;
const long intervalloSensore = 2000;

// --- GESTIONE TIMER SICUREZZA ---
const unsigned long MAX_TEMPO_ACCESO = 2 * 60 * 60 * 1000UL; // 2 ore
const unsigned long TEMPO_PAUSA       = 15 * 60 * 1000UL;     // 15 minuti

unsigned long inizioAccensione = 0;
unsigned long inizioPausa = 0;
bool inPausaForzata = false;
bool statoPrecedenteRele = RELE_SPENTO;

// Helper per verificare se l'ora attuale rientra nella fascia programmata
bool isInFasciaOraria(int oraAttuale) {
  if (!usaFasciaOraria) return true; // Se la fascia è disabilitata, ignora l'orario

  if (oraInizio < oraFine) {
    // Fascia diurna (es. dalle 8 alle 18)
    return (oraAttuale >= oraInizio && oraAttuale < oraFine);
  } else {
    // Fascia notturna a cavallo della mezzanotte (es. dalle 22 alle 7)
    return (oraAttuale >= oraInizio || oraAttuale < oraFine);
  }
}

void gestisciTimerSicurezza() {
  unsigned long correnteMillis = millis();
  bool statoAttualePin = digitalRead(relePin); 

  if (statoPrecedenteRele == RELE_SPENTO && statoAttualePin == RELE_ACCESO) {
    inizioAccensione = correnteMillis;
  }
  statoPrecedenteRele = statoAttualePin;

  // 1. GESTIONE PAUSA FORZATA ATTIVA
  if (inPausaForzata) {
    digitalWrite(relePin, RELE_SPENTO);
    if (correnteMillis - inizioPausa >= TEMPO_PAUSA) {
      inPausaForzata = false;
      if (statoDesiderato == RELE_ACCESO) {
        digitalWrite(relePin, RELE_ACCESO);
      }
    }
    return;
  }

  // 2. CONTROLLO OVERTIME
  if (statoAttualePin == RELE_ACCESO) {
    if (correnteMillis - inizioAccensione >= MAX_TEMPO_ACCESO) {
      inPausaForzata = true;
      inizioPausa = correnteMillis;
      digitalWrite(relePin, RELE_SPENTO);
    }
  } else {
    if (statoDesiderato == RELE_ACCESO) {
      digitalWrite(relePin, RELE_ACCESO);
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
    
    // Logica Modalità AUTO
    if (modalita == "AUTO") {
      time_t now;
      struct tm timeinfo;
      time(&now);
      localtime_r(&now, &timeinfo);
      int oraAttuale = timeinfo.tm_hour;

      bool fasciaValida = isInFasciaOraria(oraAttuale);

      // Si attiva solo se supera la soglia E siamo nella fascia oraria consentita
      if (temperatura >= sogliaAutomatica && fasciaValida) {
        statoDesiderato = RELE_ACCESO;
      } else if (temperatura < (sogliaAutomatica - isteresi) || !fasciaValida) {
        statoDesiderato = RELE_SPENTO;
      }

      if (!inPausaForzata) {
        digitalWrite(relePin, statoDesiderato);
      }
    }
  }
}

void handleRoot() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" charset=\"UTF-8\">";
  html += "<meta http-equiv='refresh' content='10'>";
  
  html += "<style>body { text-align: center; font-family: sans-serif; margin-top: 20px; background-color: #f4f4f9; color: #333; }";
  html += ".card { background: white; padding: 20px; max-width: 440px; margin: 15px auto; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += "a { display: inline-block; padding: 10px 18px; margin: 3px; font-size: 15px; text-decoration: none; color: white; border-radius: 5px; font-weight: bold; }";
  html += ".btn-on { background-color: #4CAF50; } .btn-off { background-color: #f44336; }";
  html += ".btn-mode { background-color: #008CBA; width: 80%; }";
  html += ".btn-soglia { background-color: #ff9800; width: 35px; font-size: 18px; padding: 4px 10px; }";
  html += ".alert { color: #d9534f; font-weight: bold; background: #f8d7da; padding: 10px; border-radius: 5px; border: 1px solid #f5c6cb; font-size: 14px; text-align: left; }";
  html += "h2 { color: #555; margin-bottom: 5px; margin-top: 5px; } .temp-val { font-size: 38px; color: #ff5722; font-weight: bold; margin: 5px 0; }";
  html += ".load-row { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #eee; padding: 12px 0; }";
  html += ".load-title { font-weight: bold; text-align: left; }";
  html += "select, input { padding: 6px; font-size: 14px; border-radius: 5px; border: 1px solid #ccc; }</style>";

  // Script JS per inviare l'ora del telefono/PC all'ESP32 alla prima apertura
  html += "<script>";
  html += "window.onload = function() {";
  html += "  if (!" + String(oraSincronizzata ? "true" : "false") + ") {";
  html += "    var d = new Date();";
  html += "    var now = Math.floor(d.getTime() / 1000);";
  html += "    fetch('/syncTime?t=' + now);";
  html += "  }";
  html += "};";
  html += "</script></head>";

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

  // Card 2: Programmazione Oraria
  html += "<div class='card'>";
  html += "<h2>Programmazione Oraria</h2>";
  if (oraSincronizzata) {
    char oraStr[10];
    sprintf(oraStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    html += "<p>Ora ESP32: <b>" + String(oraStr) + "</b></p>";
  } else {
    html += "<p style='color:orange;'>Sincronizzazione ora in corso...</p>";
  }

  html += "<form action='/setFascia' method='GET'>";
  html += "<p><label><input type='checkbox' name='attiva' value='1' " + String(usaFasciaOraria ? "checked" : "") + "> Attiva Limite Orario</label></p>";
  html += "<p>Dalle ore: <input type='number' name='inizio' min='0' max='23' value='" + String(oraInizio) + "' style='width:50px;'> ";
  html += "alle ore: <input type='number' name='fine' min='0' max='23' value='" + String(oraFine) + "' style='width:50px;'></p>";
  html += "<input type='submit' value='Salva Orari' style='background:#008CBA; color:white; border:none; padding:8px 15px; border-radius:5px; font-weight:bold; cursor:pointer;'>";
  html += "</form>";
  html += "</div>";

  // Card 3: Controllo Carico 1
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
    if (usaFasciaOraria && !isInFasciaOraria(timeinfo.tm_hour)) {
      html += "<span style='color:orange; font-size:12px;'>FUORI FASCIA</span>";
    } else {
      html += "<span style='color:gray; font-size:12px;'>Gestito in AUTO</span>";
    }
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

  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, RELE_SPENTO);

  dht.begin();
  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);

  // Sincronizza l'orologio interno dell'ESP32 tramite JavaScript
  server.on("/syncTime", []() {
    if (server.hasArg("t")) {
      time_t t = server.arg("t").toInt();
      struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
      settimeofday(&tv, NULL);
      oraSincronizzata = true;
    }
    server.send(200, "text/plain", "OK");
  });

  // Salva le impostazioni della fascia oraria
  server.on("/setFascia", []() {
    usaFasciaOraria = server.hasArg("attiva");
    if (server.hasArg("inizio")) oraInizio = server.arg("inizio").toInt();
    if (server.hasArg("fine")) oraFine = server.arg("fine").toInt();
    server.sendHeader("Location", "/"); server.send(303);
  });

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
    if (modalita == "MANUALE") {
      statoDesiderato = RELE_ACCESO;
      if (!inPausaForzata) {
        digitalWrite(relePin, RELE_ACCESO);
      }
    }
    server.sendHeader("Location", "/"); server.send(303); 
  });
  
  server.on("/off1", []() { 
    if (modalita == "MANUALE") {
      statoDesiderato = RELE_SPENTO;
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