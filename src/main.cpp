
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <time.h>
#include <Adafruit_BMP085.h>

const char* ssid = "SETUBAL";
const char* password = "Nando1977*";


const int PINO_GAS = 34;
const int PINO_SDA = 21;
const int PINO_SCL = 22;

WebServer server(80);
Adafruit_BMP085 bmp;

bool bmpDisponivel = false;

// Ultimas leituras dos sensores
float temperatura = 0.0;
float pressao = 0.0;
int leituraGas = 0;

unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO_LEITURA = 2000;



const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>EcoSense | Monitoramento</title>

<style>
* {
    box-sizing: border-box;
}

body {
    margin: 0;
    font-family: Arial, sans-serif;
    background: #101827;
    color: #f1f5f9;
}

header {
    background: #182438;
    padding: 24px;
    text-align: center;
    border-bottom: 1px solid #334155;
}

header h1 {
    margin: 0 0 8px;
    color: #67e8a5;
}

header p {
    margin: 0;
    color: #a8b5c8;
}

main {
    width: 92%;
    max-width: 1100px;
    margin: 30px auto;
}

.status {
    padding: 14px;
    background: #182438;
    border-radius: 10px;
    margin-bottom: 20px;
    color: #67e8a5;
}

.cards {
    display: grid;
    grid-template-columns: repeat(
        auto-fit, minmax(220px, 1fr)
    );
    gap: 18px;
}

.card {
    background: #1b293d;
    border: 1px solid #334155;
    border-radius: 16px;
    padding: 24px;
}

.card h2 {
    font-size: 17px;
    color: #bdc9da;
    margin-top: 0;
}

.valor {
    font-size: 34px;
    font-weight: bold;
    margin: 22px 0 10px;
    color: #67e8a5;
    overflow-wrap: anywhere;
}

.descricao {
    font-size: 13px;
    color: #a8b5c8;
    line-height: 1.5;
}

.info {
    margin-top: 24px;
    background: #182438;
    padding: 20px;
    border-radius: 12px;
    line-height: 1.7;
}

button {
    margin-top: 18px;
    padding: 12px 18px;
    background: #16a36a;
    color: white;
    border: none;
    border-radius: 8px;
    font-weight: bold;
    cursor: pointer;
}

button:hover {
    background: #128354;
}

footer {
    text-align: center;
    color: #94a3b8;
    padding: 25px;
    font-size: 13px;
}
</style>
</head>

<body>

<header>
    <h1>EcoSense</h1>
    <p>Monitoramento ambiental com ESP32</p>
</header>

<main>
    <div class="status" id="status">
        Conectando ao ESP32...
    </div>

    <section class="cards">

        <article class="card">
            <h2>🌡️ Temperatura</h2>
            <div class="valor" id="temperatura">-- °C</div>
            <div class="descricao">
                Temperatura medida pelo BMP180.
            </div>
        </article>

        <article class="card">
            <h2>🌬️ Sensor de gás</h2>
            <div class="valor" id="gas">--</div>
            <div class="descricao">
                Leitura analógica bruta do MQ-135,
                em escala de 0 a 4095.
                Não representa diretamente ppm.
            </div>
        </article>

        <article class="card">
            <h2>🌡️ Pressão barométrica</h2>
            <div class="valor" id="pressao">-- hPa</div>
            <div class="descricao">
                Pressão atmosférica medida pelo BMP180.
            </div>
        </article>

    </section>

    <section class="info">
        <strong>Horário:</strong>
        <span id="horario">Aguardando...</span>
        <br>

        <strong>Última atualização:</strong>
        <span id="atualizacao">--</span>
        <br>

        <strong>Estado do sensor:</strong>
        <span id="sensor">Verificando...</span>
        <br>

        <button onclick="atualizarDados()">
            Atualizar agora
        </button>
    </section>
</main>

<footer>
    Projeto de monitoramento com ESP32 e sensores.
</footer>

<script>
async function atualizarDados() {
    const status = document.getElementById("status");

    try {
        const resposta = await fetch("/api/dados", {
            cache: "no-store"
        });

        if (!resposta.ok) {
            throw new Error("Erro ao consultar o servidor");
        }

        const dados = await resposta.json();

        document.getElementById("temperatura").textContent =
            dados.temperatura === null
                ? "-- °C"
                : dados.temperatura.toFixed(1) + " °C";

        document.getElementById("gas").textContent =
            dados.gas;

        document.getElementById("pressao").textContent =
            dados.pressao === null
                ? "-- hPa"
                : dados.pressao.toFixed(2) + " hPa";

        document.getElementById("horario").textContent =
            dados.horario;

        document.getElementById("atualizacao").textContent =
            new Date().toLocaleTimeString("pt-BR");

        document.getElementById("sensor").textContent =
            dados.bmp_ok ? "BMP180 conectado" :
                           "BMP180 não detectado";

        status.textContent = "ESP32 conectado";
        status.style.color = "#67e8a5";

    } catch (erro) {
        status.textContent = "Não foi possível receber os dados.";
        status.style.color = "#f87171";
    }
}

// Atualiza a tela a cada 2 segundos.
atualizarDados();
setInterval(atualizarDados, 2000);
</script>

</body>
</html>
)rawliteral";


void lerSensores() {
    // Leitura analógica do MQ-135.
    leituraGas = analogRead(PINO_GAS);

    if (bmpDisponivel) {
        temperatura = bmp.readTemperature();

        pressao = bmp.readPressure() / 100.0;
    }
}


String obterHorario() {
    struct tm infoTempo;

    if (!getLocalTime(&infoTempo, 100)) {
        return "Aguardando sincronizacao";
    }

    char buffer[25];

    strftime(
        buffer,
        sizeof(buffer),
        "%d/%m/%Y %H:%M:%S",
        &infoTempo
    );

    return String(buffer);
}


void handlePagina() {
    server.send_P(
        200,
        "text/html; charset=utf-8",
        PAGINA_HTML
    );
}


void handleDados() {
    lerSensores();

    String json = "{";

    json += "\"temperatura\":";

    if (bmpDisponivel) {
        json += String(temperatura, 2);
    } else {
        json += "null";
    }

    json += ",\"pressao\":";

    if (bmpDisponivel) {
        json += String(pressao, 2);
    } else {
        json += "null";
    }

    json += ",\"gas\":";
    json += String(leituraGas);

    json += ",\"bmp_ok\":";
    json += bmpDisponivel ? "true" : "false";

    json += ",\"horario\":\"";
    json += obterHorario();
    json += "\"";

    json += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}


void setup() {
    Serial.begin(115200);

    pinMode(PINO_GAS, INPUT);
    analogReadResolution(12);

    // Inicializa o barramento I2C.
    Wire.begin(PINO_SDA, PINO_SCL);

    // Tenta detectar o BMP180.
    bmpDisponivel = bmp.begin();

    if (bmpDisponivel) {
        Serial.println("BMP180 detectado!");
    } else {
        Serial.println("BMP180 nao detectado.");
        Serial.println("Verifique as conexoes I2C.");
    }

    // Conexao Wi-Fi.
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Conectando ao Wi-Fi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi conectado!");

    Serial.print("Endereco IP: ");
    Serial.println(WiFi.localIP());

   
    configTime(
        -3 * 3600,
        0,
        "pool.ntp.org",
        "time.google.com"
    );

    server.on("/", HTTP_GET, handlePagina);
    server.on("/api/dados", HTTP_GET, handleDados);

    server.onNotFound([]() {
        server.send(404, "text/plain", "Pagina nao encontrada");
    });

    server.begin();

    Serial.println("Servidor HTTP iniciado!");
    Serial.print("Abra no navegador: http://");
    Serial.println(WiFi.localIP());
}


void loop() {
    server.handleClient();

    // Mantem leituras periodicas dos sensores.
    if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
        ultimaLeitura = millis();
        lerSensores();
    }

    delay(2);
}

