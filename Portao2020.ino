#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Update.h>

#define pinFechadura 15
#define SS_PIN 5
#define RST_PIN 4

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

WebServer server(80);

String index1 = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1>"+ INFOS +"<form method='POST' action='/arquivo' enctype='multipart/form-data'><h2><p><label>Chave: </label><input type='text' name='autorizacao'> <input type='submit'value='Ok'></p></h2></form><form method='POST' action='/abrirPortao'> <button>Abrir Portão</button></form></body></html>";
String index2 = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1>"+ INFOS +"<form method='POST'action='/update' enctype='multipart/form-data'><p><input type='file' name='update'></p><p><input type='submit' value='Atualizar'></p></form</body></html>";
String atualizado = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1><h2>Atualização bem sucedida!</h2></body></html>";
String chaveIncorreta = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1>"+ INFOS +"<h2>Chave incorreta</h2</body></html>";

MFRC522 mfrc522(SS_PIN, RST_PIN);   
int statuss = 0;
int out = 0;

hw_timer_t *timer = NULL; 

void IRAM_ATTR resetModule(){
   esp_restart(); //reinicia o chip
}


void setup(void){
  
  pinMode(pinFechadura, OUTPUT);
  digitalWrite(pinFechadura, LOW);
  
  Serial.begin(115200); 

  WiFi.mode(WIFI_STA); 

  WiFi.begin("Thuliv", "90iojknm");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.config(ip, gateway, subnet);
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  if (WiFi.status() == WL_CONNECTED) 
  {
    server.on("/", HTTP_GET, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
    });

    server.on("/arquivo", HTTP_POST, [] ()
    {
      if (server.arg("autorizacao") != "90iojknm") 
      {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", chaveIncorreta);
        //ESP.restart();
      }
      else
      {
        OTA_AUTORIZADO = true;
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", index2);
      }
    });

    server.on("/update", HTTP_POST, []()
    {
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
      else if (upload.status == UPLOAD_FILE_END)
      {
        if (Update.end(true))
        {
          Serial.printf("Atualização bem sucedida! %u\nReiniciando...\n", upload.totalSize);
        }
      }
    });

    server.on("/abrirPortao", HTTP_GET, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
      digitalWrite(pinFechadura, HIGH);
      delay(1000);
      digitalWrite(pinFechadura, LOW);
    });
    
    server.on("/abrirPortao", HTTP_POST, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
      digitalWrite(pinFechadura, HIGH);
      delay(1000);
      digitalWrite(pinFechadura, LOW);
    });

    server.begin(); 
  }

  SPI.begin();      
  mfrc522.PCD_Init(); 

  timer = timerBegin(0, 80, true); 
  
  timerAttachInterrupt(timer, &resetModule, true);
   
  timerAlarmWrite(timer, 2000000, true);
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
     digitalWrite(pinFechadura, HIGH);
     delay(1000);
     digitalWrite(pinFechadura, LOW);
    
    statuss = 1;
  } 
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}


void loop(void){
  
  server.handleClient();
  timerWrite(timer, 0);  
  RFID();
}
