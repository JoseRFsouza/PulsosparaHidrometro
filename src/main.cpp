#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- CONFIGURAÇÕES DE REDE E MQTT ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic_pub = "hidrometro/condominio";
const char* mqtt_topic_sub = "hidrometro/reset";
const char* mqtt_topic_req_config = "hidrometro/solicitar_config";
const char* mqtt_topic_res_config = "hidrometro/respostas_config";

// --- VARIÁVEIS DE CALENDÁRIO SIMULADO ---
int diaSimulado = 1;
int mesSimulado = 1;
int anoSimulado = 2026;
float ultimaHora = 0;
float fatorEscala = 144.0; // 1 min real = 144 min simulados
bool sincronizado = false; 

// --- PONDERAÇÃO E CONTADORES ---
float pesos[4] = {1.0, 4.0, 12.0, 2.0}; 
volatile float contadores[4] = {0.0, 0.0, 0.0, 0.0}; 
float contadorMestreExtra = 0;

// --- PINOS ---
const int pinoGerador = 1;
const int pinoSensorRegistro = 13; 
const int pinosCasas[4] = {2, 4, 5, 6};
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// --- ESTADOS DE OPERAÇÃO ---
bool ruaTemAgua = true;          
bool registroAberto = true;      
int unidadeVazamento = -1;       
unsigned long proximaFalhaRua = 1200000;   
unsigned long proximoVazamento = 3000000;  
unsigned long tempoRestauroRua = 0;

WiFiClient espClient;
PubSubClient client(espClient);

// --- FUNÇÃO PARA GERAR O TIMESTAMP DINÂMICO ---
String formatarDataISO() {
    char buffer[35];
    int h = (int)ultimaHora;
    int m = (int)((ultimaHora - h) * 60);
    int s = (int)((((ultimaHora - h) * 60) - m) * 60);
    // Usa as variáveis que mudam conforme o tempo simulado passa
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d.000+00:00", anoSimulado, mesSimulado, diaSimulado, h, m, s);
    return String(buffer);
}

// --- ISRs COM MULTIPLICAÇÃO PONDERADA ---
void IRAM_ATTR isr1() { if(sincronizado) { portENTER_CRITICAL_ISR(&mux); contadores[0] += pesos[0]; portEXIT_CRITICAL_ISR(&mux); } }
void IRAM_ATTR isr2() { if(sincronizado) { portENTER_CRITICAL_ISR(&mux); contadores[1] += pesos[1]; portEXIT_CRITICAL_ISR(&mux); } }
void IRAM_ATTR isr3() { if(sincronizado) { portENTER_CRITICAL_ISR(&mux); contadores[2] += pesos[2]; portEXIT_CRITICAL_ISR(&mux); } }
void IRAM_ATTR isr4() { if(sincronizado) { portENTER_CRITICAL_ISR(&mux); contadores[3] += pesos[3]; portEXIT_CRITICAL_ISR(&mux); } }

void callback(char* topic, byte* payload, unsigned int length) {
    String topico = String(topic);
    String mensagem = "";
    for (int i = 0; i < length; i++) mensagem += (char)payload[i];

    Serial.println("Mensagem recebida no topico: " + topico);
    Serial.println("Conteudo: " + mensagem);

    // --- LOGICA DE RESET (Vindo do Frontend) ---
    if (topico == mqtt_topic_sub) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, mensagem);

        if (!error) {
            // Se o frontend enviar {"unit": X}
            if (doc.containsKey("unit")) {
                int unidadeParaResetar = doc["unit"]; // Pega o número da unidade (1, 2, 3 ou 4)
                
                Serial.print("Resetando alerta da unidade: ");
                Serial.println(unidadeParaResetar);

                // IMPORTANTE: O sistema usa unidadeVazamento de 0 a 3 internamente
                // Se a unidade vindo do front for a que está vazando, limpamos o erro
                if (unidadeVazamento == (unidadeParaResetar - 1)) {
                    unidadeVazamento = -1; 
                    Serial.println("Vazamento interrompido via software.");
                }
            }
        } else {
            // Caso você queira manter o suporte ao texto puro "RESET" para testes manuais
            if (mensagem == "RESET") {
                unidadeVazamento = -1;
                Serial.println("Reset Geral de Vazamento acionado.");
            }
        }
    }

    // --- LOGICA DE SINCRONIZAÇÃO INICIAL (Mantida) ---
    if (topico == mqtt_topic_res_config && !sincronizado) {
        StaticJsonDocument<512> doc;
        if (!deserializeJson(doc, mensagem)) {
            portENTER_CRITICAL(&mux);
            contadores[0] = doc["c1"] | 0.0;
            contadores[1] = doc["c2"] | 0.0;
            contadores[2] = doc["c3"] | 0.0;
            contadores[3] = doc["c4"] | 0.0;
            portEXIT_CRITICAL(&mux);

            if(doc.containsKey("ts")) {
                String ts = doc["ts"].as<String>();
                if(ts.length() >= 19) {
                    anoSimulado = ts.substring(0, 4).toInt();
                    mesSimulado = ts.substring(5, 7).toInt();
                    diaSimulado = ts.substring(8, 10).toInt();
                    ultimaHora = ts.substring(11, 13).toInt() + (ts.substring(14, 16).toInt() / 60.0);
                }
            }
            sincronizado = true;
            Serial.println("Sincronizado com sucesso!");
        }
    }
}

void setup_wifi() {
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi OK");
}

void reconnect() {
    while (!client.connected()) {
        String clientId = "ESP32_Hidro_" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            client.subscribe(mqtt_topic_sub);
            client.subscribe(mqtt_topic_res_config);
            client.publish(mqtt_topic_req_config, "{\"req\":\"get_latest\"}");
        } else { delay(5000); }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(pinoGerador, OUTPUT);
    pinMode(pinoSensorRegistro, INPUT_PULLUP);

    for(int i=0; i<4; i++) {
        pinMode(pinosCasas[i], INPUT_PULLDOWN);
        if(i==0) attachInterrupt(digitalPinToInterrupt(pinosCasas[i]), isr1, RISING);
        else if(i==1) attachInterrupt(digitalPinToInterrupt(pinosCasas[i]), isr2, RISING);
        else if(i==2) attachInterrupt(digitalPinToInterrupt(pinosCasas[i]), isr3, RISING);
        else if(i==3) attachInterrupt(digitalPinToInterrupt(pinosCasas[i]), isr4, RISING);
    }
    
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) setup_wifi();
    if (!client.connected()) reconnect();
    client.loop();

    if (sincronizado) {
        unsigned long now = millis();
        unsigned long tempoSimuladoMS = now * fatorEscala;
        float horaSimulada = fmod((tempoSimuladoMS / 3600000.0), 24.0);

        if (horaSimulada < ultimaHora) {
            diaSimulado++;
            if (diaSimulado > 30) { diaSimulado = 1; mesSimulado++; }
        }
        ultimaHora = horaSimulada;
        registroAberto = (digitalRead(pinoSensorRegistro) == HIGH);

        // --- LÓGICA DE FALHAS ---
        if (now > proximaFalhaRua) {
            proximaFalhaRua = now + 1200000;
            if (random(0, 100) < 30) {
                ruaTemAgua = false;
                tempoRestauroRua = now + random(25000, 200000);
            }
        }
        if (!ruaTemAgua && now > tempoRestauroRua) ruaTemAgua = true;

        if (unidadeVazamento == -1 && now > proximoVazamento) {
            proximoVazamento = now + 3000000;
            unidadeVazamento = random(0, 4);
        }

        // --- GERADOR DE PULSOS ---
        int freqHz = (horaSimulada >= 17 && horaSimulada < 21) ? 60 : (horaSimulada >= 11 && horaSimulada < 14 ? 30 : 5);

        if (ruaTemAgua && registroAberto) {
            static unsigned long lastGen = 0;
            unsigned long pGen = (freqHz > 0) ? (1000 / freqHz) : 0;
            if (pGen > 0 && now - lastGen >= pGen) {
                digitalWrite(pinoGerador, HIGH);
                delayMicroseconds(10); 
                digitalWrite(pinoGerador, LOW);
                lastGen = now;
            }

            // Consumo Área Comum (Terça=2 e Sábado=6)
            int diaSemana = diaSimulado % 7;
            static unsigned long lastMestre = 0;
            bool areaComum = (diaSemana == 2 || diaSemana == 6) && (horaSimulada >= 7 && horaSimulada < 14);
            if (areaComum && now - lastMestre >= 500) {
                contadorMestreExtra += 1.0; 
                lastMestre = now;
            }
        }

        // Vazamento Ponderado
        static unsigned long lastLeak = 0;
        if (unidadeVazamento != -1 && ruaTemAgua && registroAberto && now - lastLeak >= 1000) {
            portENTER_CRITICAL(&mux); 
            contadores[unidadeVazamento] += pesos[unidadeVazamento]; 
            portEXIT_CRITICAL(&mux);
            lastLeak = now;
        }

        // --- ENVIO MQTT COM TIMESTAMP SIMULADO REAL ---
        static unsigned long lastSend = 0;
        if (now - lastSend > 5000) {
            lastSend = now;
            float totalCasas = contadores[0] + contadores[1] + contadores[2] + contadores[3];
            String statusAbs = (!registroAberto) ? "REGISTRO_FECHADO" : (!ruaTemAgua ? "FALTA_DE_AGUA_NA_RUA" : "SISTEMA_OK");

            String payload = "{";
            payload += "\"timestamp\":\"" + formatarDataISO() + "\","; // Agora dinâmico!
            for(int i=0; i<4; i++) payload += "\"c" + String(i+1) + "\":" + String(contadores[i], 1) + ",";
            payload += "\"status_abastecimento\":\"" + statusAbs + "\",";
            payload += "\"vazamento_ativo\":" + String(unidadeVazamento + 1) + ",";
            payload += "\"consumo_mestre\":" + String(totalCasas + contadorMestreExtra, 1) + ",";
            payload += "\"consumo_total\":" + String(totalCasas, 1);
            payload += "}";
            
            client.publish(mqtt_topic_pub, payload.c_str());
        }
    } else {
        // Se não sincronizou ainda, tenta pedir os dados a cada 10 segundos
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry > 10000) {
            lastRetry = millis();
            client.publish(mqtt_topic_req_config, "{\"req\":\"get_latest\"}");
            Serial.println("Aguardando sincronia...");
        }
    }
}