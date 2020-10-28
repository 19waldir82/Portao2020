#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

#define pinFechadura 17
#define SDA_PIN 21
#define RST_PIN 22

const char* ssid = "Thuliv";
const char* password = "90iojknm";

IPAddress ip(192, 168, 1, 253);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

//Algumas informações que podem ser interessantes
const uint32_t chipID = (uint32_t)(ESP.getEfuseMac() >> 32); //um ID exclusivo do Chip...
const String CHIP_ID = "<p> Chip ID: " + String(chipID) + "</p>"; // montado para ser usado no HTML
const String VERSION = "<p> Versão: 1.0 </p>"; //Exemplo de um controle de versão

//Informações interessantes agrupadas
const String INFOS = VERSION + CHIP_ID;

boolean OTA_AUTORIZADO = false;
unsigned long tempo = 0;

WebServer server(80);

String index1 =
"<!DOCTYPE html>"
  "<html>"
    "<head>"
      "<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>"
      "<title>ThulivTEC Gate</title>"
      "<meta charset='UTF-8'>"

      "<style>"
        "body{"
          "text-align: center;"
          "font-family: sans-serif;"
          "font-size:14px;"
          "padding: 25px;"
        "}"

        "p{"
          "color:#444;"
        "}"

        "button{"
          "outline: none;"
          "border: 2px solid #1fa3ec;"
          "border-radius:18px;"
          "background-color:#FFF;"
          "color: #1fa3ec;"
          "padding: 10px 50px;"
        "}"

        "button:active{"
          "color: #FFF;"
          "background-color:#F60;"
        "}"
      "</style>"
    "</head>"
    
    "<body>"
      "<h1>ThulivTEC Gate</h1>"
      + INFOS +
      "<form method='POST' action='/arquivo' enctype='multipart/form-data'>"
      "<label>Chave: </label><input type='text' name='autorizacao'> <input type='submit'value='Ok'></form>"
      
      "<p><form method='POST' action='/abrirPortao'> <button>Abrir Portão</button></form></p>"
    "</body>"
  "</html>";
String index2 = "<!DOCTYPE html><html><head><title>ThulivTEC Gate</title><meta charset='UTF-8'></head><body><h1>ThulivTEC Gate</h1>"+ INFOS +"<form method='POST'action='/update' enctype='multipart/form-data'><p><input type='file' name='update'></p><p><input type='submit' value='Atualizar'></p></form</body></html>";
String atualizado = "<!DOCTYPE html><html><head><title>ThulivTEC Gate</title><meta charset='UTF-8'></head><body><h1>ThulivTEC Gate</h1><h2>Atualização bem sucedida!</h2></body></html>";
String chaveIncorreta = "<!DOCTYPE html><html><head><title>ThulivTEC Gate</title><meta charset='UTF-8'></head><body><h1>ThulivTEC Gate</h1>"+ INFOS +"<h2>Chave incorreta</h2</body></html>";

MFRC522 mfrc522(SDA_PIN, RST_PIN);   
int statuss = 0;
int out = 0;

hw_timer_t *timer = NULL; 

void IRAM_ATTR resetModule(){
   esp_restart(); //reinicia o chip
}


void setup(void)
{
  pinMode(pinFechadura, OUTPUT);
  
  Serial.begin(115200); 

  WiFi.mode(WIFI_AP_STA); 

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.config(ip, gateway, subnet);
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  MDNS.begin("talkinghome");
 

  if (WiFi.status() == WL_CONNECTED) 
  {
    server.on("/", HTTP_GET, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
    });

    server.on("/arquivo", HTTP_POST, [] ()
    {
      Serial.println("Em server.on /avalia: args= " + String(server.arg("autorizacao"))); 

      if (server.arg("autorizacao") != "90iojknm") 
      {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", chaveIncorreta);
      }
      else
      {
        OTA_AUTORIZADO = true;
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", index2);
      }
    });

    server.on("/index2", HTTP_GET, []()
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index2);
    });

    server.on("/update", HTTP_POST, []()
    {
      if (OTA_AUTORIZADO == false)
      {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", chaveIncorreta);
        return;
      }
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", (Update.hasError()) ? chaveIncorreta : atualizado);
      delay(1000);
      ESP.restart();
    }, []()
    {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START)
      {
        Serial.setDebugOutput(true);
        Serial.printf("Atualizando: %s\n", upload.filename.c_str());
        if (!Update.begin())
        {
          Update.printError(Serial);
        }
      }
      else if (upload.status == UPLOAD_FILE_WRITE)
      {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
          Update.printError(Serial);
        }
      }
      else if (upload.status == UPLOAD_FILE_END)
      {
        if (Update.end(true))
        {
          Serial.printf("Atualização bem sucedida! %u\nReiniciando...\n", upload.totalSize);
        }
        else
        {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      else
      {
        Serial.printf("Atualização falhou inesperadamente! (possivelmente a conexão foi perdida.): status=%d\n", upload.status);
      }
    });

    server.on("/abrirPortao", HTTP_GET, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
      tempo = millis();
      digitalWrite (pinFechadura, HIGH);
    });
    
    server.on("/abrirPortao", HTTP_POST, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
      tempo = millis();
      digitalWrite (pinFechadura, HIGH);
    });

    server.begin(); //inicia o servidor
  }
  
  SPI.begin();      
  mfrc522.PCD_Init(); 

  timer = timerBegin(0, 80, true); 
  
  timerAttachInterrupt(timer, &resetModule, true);
   
  timerAlarmWrite(timer, 30000000, true);
  timerAlarmEnable(timer); 
}


void RFID(){
  
  if ( ! mfrc522.PICC_IsNewCardPresent()){
    return;
  }

  if ( ! mfrc522.PICC_ReadCardSerial()){
    return;
  }

  String content = "";
  
  for (byte i = 0; i < mfrc522.uid.size; i++){
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  
  content.toUpperCase();
  
  if ((content == "A63FF921") || (content == "D0D6652B") || (content == "A6ACCF24") || (content == "2967845A") || (content == "A318E636") || (content == "A320AC36") || (content == "D0A4882B") || (content == "F97EDAA3") || (content == "B61E7021") || (content == "8309EE36")) {    
     tempo = millis();
     digitalWrite(pinFechadura, HIGH);
     statuss = 1;
  } 
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}


void loop(void)
{
  server.handleClient();
  timerWrite(timer, 0);
  RFID();

  if((millis() - tempo) > 500){
    digitalWrite(pinFechadura, LOW);
    }
}
