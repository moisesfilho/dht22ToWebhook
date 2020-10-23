#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <SPI.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>

#ifndef STASSID
#define STASSID ""
#define STAPSK ""
#endif

const char *ssid = STASSID;
const char *password = STAPSK;
uint8_t pin_dht = D3;
const long utcOffsetInSeconds = -3 * 60 * 60;

ESP8266WebServer server(80);
DHT dht(pin_dht, DHT22);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "south-america.pool.ntp.org", utcOffsetInSeconds);

float minutos = 1;

String getDateTime()
{
  unsigned long epochTime = timeClient.getEpochTime();
  String time = timeClient.getFormattedTime();

  struct tm *ptm = gmtime((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;

  int currentMonth = ptm->tm_mon + 1;

  int currentYear = ptm->tm_year + 1900;

  String dateTime = String(monthDay) + "/" + String(currentMonth) + "/" + String(currentYear) + " " + time;

  return dateTime;
}

String getTemperature()
{
  float t = dht.readTemperature();
  float temperature = isnan(t) ? 0 : t;
  String temperature_string = String(temperature);
  temperature_string.replace(".", ",");
  return temperature_string;
}

String getHumidity()
{
  float h = dht.readHumidity();
  float humidity = isnan(h) ? 0 : h;
  String humidity_string = String(humidity);
  humidity_string.replace(".", ",");
  return humidity_string;
}

String sendData()
{
  String temperature = getTemperature();

  String humidity = getHumidity();

  String dateTime = getDateTime();

  String values = "{\"value1\":\"" + temperature + "\",\"value2\":\"" + humidity + "\",\"value3\":\"" + dateTime + "\"}";

  HTTPClient http;
  http.begin("http://maker.ifttt.com/trigger/dados/with/key/k5bq9RP7lhyBoNLlGZNE3");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(values);

  String httpReturn = "erro";

  if (httpCode > 0)
  {
    httpReturn = http.getString();
  }

  return httpReturn;
}

void handleRoot()
{
  digitalWrite(LED_BUILTIN, 1);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(LED_BUILTIN, 0);
}

void handleNotFound()
{
  digitalWrite(LED_BUILTIN, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(LED_BUILTIN, 0);
}

void temperature()
{
  String temperature = getTemperature();
  server.send(200, "text/plain", temperature);
}

void humidity()
{
  String humidity = getHumidity();
  server.send(200, "text/plain", humidity);
}

void data()
{
  String temperature = getTemperature();
  String humidity = getHumidity();

  String htmlPage;
  htmlPage += "<html>\r\n";
  htmlPage += "<style>\r\n";
  htmlPage += "  h1 {text-align: center;vertical-align: middle;}\r\n";
  htmlPage += "</style>\r\n";
  htmlPage += "<style>\r\n";
  htmlPage += " h1 {";
  htmlPage += "   text-align: center";
  htmlPage += " }";
  htmlPage += " .center {";
  htmlPage += "   margin: 0;";
  htmlPage += "   position: absolute;";
  htmlPage += "   top: 50%;";
  htmlPage += "   left: 50%;";
  htmlPage += "   -ms-transform: translate(-50%, -50%);";
  htmlPage += "   transform: translate(-50%, -50%);";
  htmlPage += "  }";
  htmlPage += "</style>\r\n";
  htmlPage += "<head><title>dados</title></head>\r\n";
  htmlPage += "  <div class='center'>\r\n";
  htmlPage += "    <h1>Temperatura: " + temperature + "'</h1>\r\n";
  htmlPage += "    <h1>Umidade: " + humidity + "%</h1>\r\n";
  htmlPage += "  </div>\r\n";
  htmlPage += "</html>";

  server.send(200, "text/html", htmlPage);
}

void webhook()
{
  String retorno = sendData();
  server.send(200, "text/plain", retorno);
}

void setIntervaloMinutos()
{
  String message = "";

  if (server.arg("minutos") == "")
  {
    message = "Parâmetro minutos não encontrado";
  }
  else
  {
    minutos = server.arg("minutos").toFloat();

    if (minutos < 0.5)
    {
      minutos = 0.5;
    }

    message = "Minutos = ";
    message += minutos;
  }

  server.send(200, "text/plain", message);
}

void setup(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266"))
  {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/temperature", temperature);
  server.on("/humidity", humidity);
  server.on("/data", data);
  server.on("/webhook", webhook);
  server.on("/intervalo", setIntervaloMinutos);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  pinMode(pin_dht, INPUT);
  dht.begin();

  timeClient.begin();
}

unsigned long previousMillis = 0;

void timer()
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= minutos * 60 * 1000)
  {
    previousMillis = currentMillis;

    sendData();
  }
}

void loop(void)
{
  server.handleClient();
  MDNS.update();
  timeClient.update();
  timer();
}