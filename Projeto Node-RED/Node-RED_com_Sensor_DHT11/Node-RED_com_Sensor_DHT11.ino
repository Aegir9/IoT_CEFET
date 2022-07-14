#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>

// defines - Mapeamento de pinos do NodeMCU
#define D0 16
#define D1 5 // LEDPIN
#define D2 4
#define D3 0
#define D4 2 // DHTPIN
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3
#define D10 1

// DHT
#define DHTTYPE DHT11
DHT dht(D4, DHTTYPE);

// WIFI
const char *myHostname = "ESP8266_MDS"; // Nome do host na rede e do OTA
const char *SSID = "ESP_MDS_AP";        // SSID / nome da rede WI-FI (AP) do WiFiManager
const char *PASSWORD_AP = "12121215";   // Senha da rede WI-FI (AP) do WiFiManager
const char *PASSWORD_OTA = "25696969";  // Senha de acesso a Porta de Rede OTA

// MQTT
#define TOPICO_SUBSCRIBE_LED1 "TarefaNodeRed/LED-BUTTON"           // tópico MQTT de escuta luz 1
#define TOPICO_PUBLISH_DHT_UMIDADE "TarefaNodeRed/umidade"         // tópico MQTT de publicar umidade
#define TOPICO_PUBLISH_DHT_TEMPERATURA "TarefaNodeRed/temperatura" // tópico MQTT de publicar temperatura

#define ID_MQTT "ESPMDS-26" // id mqtt (para identificação de sessão)
                            // IMPORTANTE: este deve ser único no broker (ou seja,
                            //            se um client MQTT tentar entrar com o mesmo
                            //            id de outro já conectado ao broker, o broker
                            //            irá fechar a conexão de um deles).

#define USER_MQTT "" // usuario no MQTT
#define PASS_MQTT "" // senha no MQTT

const char *BROKER_MQTT = "test.mosquitto.org"; // URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                         // Porta do Broker MQTT

// Variáveis e objetos globais
WiFiClient espClient;         // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient
int ultimoEnvioMQTT = 0;
int segundos = 10; // Tempo de atualização das informações do sensor em segundos. 

// Protótipos de Função
void initSerial();
void initWifiAp();
void initOTA();
void initMQTT();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void VerificaConexoesWiFIEMQTT();
void initOutput();
void initSensorDHT11();
void envioMQTTPorTempo(int intervaloEnvioMS);

void setup()
{
    initSerial();
    initWifiAp();
    initOTA();
    initMQTT();
    initOutput();
    initSensorDHT11();
    Serial.println("Programa carregado!");
    Serial.println();
}

void loop()
{
    ArduinoOTA.handle();    // keep-alive da comunicação OTA
    VerificaConexoesMQTT(); // garante funcionamento da conexão com o broker MQTT.

    envioMQTTPorTempo(segundos) ; 

    MQTT.loop(); // keep-alive da comunicação com broker MQTT
}

// Função: inicializa comunicação serial com baudrate 115200 (para fins de monitorar no terminal serial
//         o que está acontecendo.
// Parâmetros: nenhum
// Retorno: nenhum
void initSensorDHT11()
{
    dht.begin();
    Serial.println("Iniciou o sensor DHT11.");
}

// Função: inicializa comunicação serial com baudrate 115200 (para fins de monitorar no terminal serial
//         o que está acontecendo.
// Parâmetros: nenhum
// Retorno: nenhum
void initSerial()
{
    Serial.begin(115200);
}

// Função: inicializa WifiManager - cria Ponto de acesso e permite configurar a
// conexão WiFi quando não estiver conectado.
// Parâmetros: nenhum
// Retorno: nenhum
void initWifiAp()
{
    WiFiManager wifiManager;

    // wifiManager.resetSettings(); //Usado para resetar SSID e SENHAS armazenadas do WiFiManager
    wifiManager.setClass("invert");
    wifiManager.setHostname(myHostname);
    wifiManager.autoConnect(SSID, PASSWORD_AP);

    Serial.println();
    Serial.print("Conectado com sucesso na rede via WifiManager na rede: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP obtido: ");
    Serial.println(WiFi.localIP()); // Mostra o endereço IP obtido via DHCP
    Serial.print("Endereço MAC: ");
    Serial.println(WiFi.macAddress()); // Mostra o endereço MAC do ESP8266
    Serial.print("Hostname: ");
    Serial.println(WiFi.hostname());
    Serial.println();
}

// Função inicializa OTA - permite carga do novo programa via Rede.
// Parâmetros: nenhum
// Retorno: nenhum
void initOTA()
{
    Serial.println();
    Serial.println("Iniciando OTA....");
    ArduinoOTA.setHostname(myHostname); // Define o nome da porta

    // No authentication by default
    ArduinoOTA.setPassword(PASSWORD_OTA); // Senha para carga via Rede (OTA)
    ArduinoOTA.onStart([]()
                       { Serial.println("Start"); });
    ArduinoOTA.onEnd([]()
                     { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { 
                              Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                              Serial.println(); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
    ArduinoOTA.begin();
}

// Função: inicializa parâmetros de conexão MQTT(endereço do broker, porta e seta função de callback)
// Parâmetros: nenhum
// Retorno: nenhum
void initMQTT()
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT); // informa qual broker e porta deve ser conectado
    MQTT.setCallback(mqtt_callback);          // atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}

// Função: função de callback,
//         esta função é chamada toda vez que uma informação de
//         um dos tópicos subescritos chega)
// Parâmetros: nenhum
// Retorno: nenhum
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    String msg;

    // obtem a string do payload recebido
    for (int i = 0; i < length; i++)
    {
        char c = (char)payload[i];
        msg += c;
    }

    Serial.println("msg = " + msg);

    if (msg.equals("OFF"))// Desliga o led. 
    {
        digitalWrite(D1, LOW);
        Serial.println("LED Desligado");
    }

    if (msg.equals("ON"))// Liga o led.
    {
        digitalWrite(D1, HIGH);
        Serial.println("LED Ligado");
    }
}

// Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
//         em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
// Parâmetros: nenhum
// Retorno: nenhum
void reconnectMQTT()
{
    while (!MQTT.connected())
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        // if (MQTT.connect(ID_MQTT, USER_MQTT,PASS_MQTT))      // Parameros usados para broker proprietário
                                                                // ID do MQTT, login do usuário, senha do usuário

        if (MQTT.connect(ID_MQTT))
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            MQTT.subscribe(TOPICO_SUBSCRIBE_LED1);
        }
        else
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(2000);
        }
    }
}

// Função: verifica o estado da conexão ao broker MQTT.
//         Em caso de desconexão, a conexão é refeita.
// Parâmetros: nenhum
// Retorno: nenhum
void VerificaConexoesMQTT()
{
    if (!MQTT.connected())
        reconnectMQTT(); // se não há conexão com o Broker, a conexão é refeita
}

// Função: inicializa o output em nível lógico baixo
// Parâmetros: nenhum
// Retorno: nenhum
void initOutput()
{
    // Enviar HIGH para o output faz o Led acender / enviar LOW faz o Led apagar).
    // LEDs iniciam apagados.
    pinMode(D1, OUTPUT);
    digitalWrite(D1, LOW);
}
// Função: realiza a leitura de umidade e temperatura do sensor DHT e publica
//         os resultados no tópico MQTT definido.
// Parâmetros: nenhum
// Retorno: nenhum
void enviaDHT()
{
    char MsgUmidadeMQTT[10];
    char MsgTemperaturaMQTT[10];

    float umidade = dht.readHumidity();
    float temperatura = dht.readTemperature();

    if (isnan(temperatura) || isnan(umidade))
    {
        Serial.println("Falha na leitura do DHT11...");
    }
    else
    {
        Serial.print("Umidade: ");
        Serial.println(umidade);
        Serial.print("Temperatura: ");
        Serial.print(temperatura);
        Serial.println(" °C");

        sprintf(MsgUmidadeMQTT, "%.2f", umidade);
        MQTT.publish(TOPICO_PUBLISH_DHT_UMIDADE, MsgUmidadeMQTT);
        sprintf(MsgTemperaturaMQTT, "%.2f", temperatura);
        MQTT.publish(TOPICO_PUBLISH_DHT_TEMPERATURA, MsgTemperaturaMQTT);
    }
}

// Função: chama as funções definidas internamente a cada X segundos, definidos via parâmetro.
// Parâmetros: (int) intervalo em segundos para o envio das mensagens
// Retorno: nenhum
void envioMQTTPorTempo(int intervaloEnvioSegundos)
{
    // envia a cada X segundos
    if ((millis() - ultimoEnvioMQTT) > (intervaloEnvioSegundos * 1000))
    {
        enviaDHT();
        ultimoEnvioMQTT = millis();
    }
}
