
// Import required libraries
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <ESP32Ping.h>
#endif
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include "ModbusRtu.h";
#include <vector>
#include<String>
#include<ArduinoJson.h>
#include<PubSubClient.h>
#include "esp_task_wdt.h"
#include <ESP32_FTPClient.h>

//char ftp_server[] = "192.168.100.50";
//char ftp_user[]   = "esp32";
//char ftp_pass[]   = "123456789";
//char ftp_filename[] = "";
//char ftp_directory[] = "";

String ftp_server = "";
String ftp_user = "";
String ftp_pass = "";
String ftp_mcstatus_filename = "";
String ftp_mcstatus_directory = "";
String ftp_mc_alarmlist_filename = "";
String ftp_mc_alarmlist_directory = "";


// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp ("", "", "", 5000, 2);


// Replace with your network credentials
//const char* ssid = "SMM";
//const char* password = "mic@admin";

// Set LED GPIO
#define led_connection 42//old is 2
#define led_run 41//old is 1
// Stores LED state
String ledState;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);




const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* PARAM_INPUT_5 = "subnet";
const char* PARAM_INPUT_6 = "hostname";
const char* PARAM_INPUT_7 = "dhcpchecked";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;
String subnetString;
String hostname = "SMM";
String dhcpchecked = "false";

String username = "admin";
String password = "admin";
String mqttAddress = "";
String mqttTopic = "";
int mqttPort = 1883;
int mqttInterval;
String mqttEnable;

//String readTopics = "";

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";
const char* subnetStringPath = "/subnetString.txt";
const char* hostnamePath = "/hostname.txt";
const char* dhcpcheckedPath = "/dhcpchecked.txt";

const char* modbusPath = "/modbusPath.txt";
const char* portPath = "/portPath.txt";
const char* baudRatePath = "/baudRatePath.txt";
const char* databitPath = "/databitPath.txt";
const char* stopbitPath = "/stopbitPath.txt";
const char* paritybitPath = "/paritybitPath.txt";
const char* eventIntervalPath = "/eventIntervalPath.txt";
const char* modbusAddressPath = "/modbusAddressPath.txt";


const char* usernamePath = "/usernamePath.txt";
const char* passwordPath = "/passwordPath.txt";
const char* mqttAddressPath = "/mqttAddressPath.txt";
const char* mqttPortPath = "/mqttPortPath.txt";
const char* mqttIntervalPath = "/mqttIntervalPath.txt";
const char* mqttEnablePath = "/mqttEnablePath.txt";
const char* mqttTopicPath = "/mqttTopicPath.txt";

const char* ftp_server_path = "/ftp_server_path.txt";
const char* ftp_user_path = "/ftp_user_path.txt";
const char* ftp_pass_path = "/ftp_pass_path.txt";
const char* ftp_mcstatus_filename_path = "/ftp_mcstatus_filename_path.txt";
const char* ftp_mcstatus_directory_path = "/ftp_mcstatus_path.txt";
const char* ftp_mc_alarmlist_filename_path = "/ftp_mc_alarmlist_name_path.txt";
const char* ftp_mc_alarmlist_directory_path = "/ftp_mc_alarmlist_path.txt";



const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <title>ESP Web Server</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <script type="text/javascript">
        function doSomething() {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", "/restartESP32", true);
            xhr.send();
        }
        window.onload = doSomething;
    </script>

</head>

<body>

    Done. ESP will restart, connect to your router and go to IP address:
    %REQUESTDHCPIP%
    <br><br>
    If connection could not be established or the desired IP address has already been assigned. <br>Please try reconnect
    to ESP32 and go to IP address 192.168.4.1 again.
</body>

</html>
)rawliteral";


IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 255, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 2000;  // interval to wait for Wi-Fi connection (milliseconds)
const long intervalBus = 1000;

int num;
int rowNum;
int addressNumIdx = 1;
std::vector<uint16_t> Data;

int baudRate = 9600;
int modbusAddress = 1;
String databit = "8bit";
String stopbit = "1bit";
String paritybit = "none";

String modbus = "slave";
String port = "rs232";

Modbus slave(modbusAddress, Serial1, 0);

bool isUsed;

unsigned long eventInterval = 1000;
unsigned long previousTime = 0;



WiFiClient espClient;
PubSubClient client(espClient);

String topics[63] = {
  "","","","","","","","","","",
  "","","","","","","","","","",
  "","","","","","","","","","",
  "","","","","","","","","","",
  "","","","","","","","","","",
  "","","","","","","","","","",
  "","","",
};

bool isMQTTConnected;

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "REQUESTDHCPIP"){
    String ip = "";
    ip = WiFi.localIP().toString();
    return ip;
  }
  return String();
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void printCharArray(String str, char array[]) {
  for (int i = 0; i < sizeof(array); i++) {
    array[i] = str[i];
    Serial.print(array[i]);
  }
}


// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

String readFileWithLine(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
    while (file.available()) {
        fileContent = file.readString();   
    }

  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

//append file to SPIFFS
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){

      Serial.println("- message appended");
    } else {
      Serial.println("- append failed");
    }
    file.close();
}



bool wifiIntialized;

// Initialize WiFi
bool initWiFi() {
  Serial.println(dhcpchecked);
  if ((ssid == "" || ip == "") && dhcpchecked != "true") {
    Serial.println("Undefined SSID or IP address.");
    wifiIntialized = false;
    return false;

  }
  if (isUsed) {
    isUsed = false;
    wifiIntialized = false;
    return false;
  }
  if (dhcpchecked == "true") {
    //WiFi.mode(WIFI_STA);

  } else {

    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    subnet.fromString(subnetString.c_str());
    if (!WiFi.config(localIP, localGateway, subnet)) {
      Serial.println("STA Failed to configure");
      wifiIntialized = false;
      return false;

    }



  }

  //  localIP.fromString(ip.c_str());
  //  localGateway.fromString(gateway.c_str());
  //  subnet.fromString(subnetString.c_str());
  //  if (!WiFi.config(localIP, localGateway, subnet)) {
  //    Serial.println("STA Failed to configure");
  //    wifiIntialized = false;
  //    return false;
  //
  //  }


  //Serial.print(ssid.c_str());Serial.print(pass.c_str());
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      wifiIntialized = false;
      writeFile(SPIFFS, ssidPath, NULL);
      writeFile(SPIFFS, ipPath, NULL);
      dhcpchecked = "false";
      writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());
      delay(100);
      ESP.restart();
      
      return false;

    }
  }

  Serial.println(WiFi.localIP());
  wifiIntialized = true;
  return true;
}


String masterParamSettings[8] = {"modbus", "port", "baud", "databit", "stopbit", "parity", "timeoutTime", "modbusAddress"};

String concate = "";

#include <string>

bool isConnected;
unsigned long prevCount = 60000;
unsigned long intervalCount = 60000;


void setup() {
    
//     bool success = Ping.ping(ip.c_str(), 1);
//        //Serial.print(success);
//    if (success) {
//          
//      Serial.println("IP used");
//      ip = "";
//      //ssid = "";
//      writeFile(SPIFFS, ssidPath, NULL);
//      //ip = "";
//      writeFile(SPIFFS, ipPath, NULL);
//
//      dhcpchecked = "false";
//      writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());
//      delay(100);
//      ESP.restart();
//
//      }
  

   

   
   Data.resize(64);
  // Serial port for debugging purposes
   Serial.begin(115200);

  
   Serial.println("ESP32 RUNNING");
    
  
  

  
//  Serial1.begin(baudRate, SERIAL_8N1);

  pinMode(led_connection, OUTPUT);
  pinMode(led_run, OUTPUT);
//
//  digitalWrite(led_connection, HIGH);
  digitalWrite(led_run, HIGH);
  digitalWrite(led_connection, HIGH);

  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Read_MC_Status();
  Read_Alarm_List();
  ReadTopic();
  // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  gateway = readFile (SPIFFS, gatewayPath);
  subnetString = readFile(SPIFFS, subnetStringPath);
  hostname = readFile(SPIFFS, hostnamePath);
  dhcpchecked = readFile(SPIFFS, dhcpcheckedPath);

  modbus = readFile(SPIFFS, modbusPath);
  port = readFile(SPIFFS, portPath);
  baudRate = readFile(SPIFFS, baudRatePath).toInt();
  databit = readFile(SPIFFS, databitPath);
  stopbit = readFile(SPIFFS, stopbitPath);
  paritybit = readFile(SPIFFS, paritybitPath);
  eventInterval = strtol(readFile(SPIFFS, eventIntervalPath).c_str(), NULL, 10);
  modbusAddress = readFile(SPIFFS, modbusAddressPath).toInt();
  
  username = readFile(SPIFFS, usernamePath);
  password = readFile(SPIFFS, passwordPath);
  mqttTopic = readFile(SPIFFS, mqttTopicPath);
  mqttAddress = readFile(SPIFFS, mqttAddressPath);
  mqttPort = readFile(SPIFFS, mqttPortPath).toInt();
  mqttInterval = readFile(SPIFFS, mqttIntervalPath).toInt();
  mqttEnable = readFile(SPIFFS, mqttEnablePath);

  ftp_server = readFile(SPIFFS, ftp_server_path);
  ftp_user = readFile(SPIFFS, ftp_user_path);
  ftp_pass = readFile(SPIFFS, ftp_pass_path);
  ftp_mcstatus_filename = readFile(SPIFFS, ftp_mcstatus_filename_path);
  ftp_mcstatus_directory = readFile(SPIFFS, ftp_mcstatus_directory_path);
  ftp_mc_alarmlist_filename = readFile(SPIFFS, ftp_mc_alarmlist_filename_path);
  ftp_mc_alarmlist_directory = readFile(SPIFFS, ftp_mc_alarmlist_directory_path);


  SetupFTP(ftp_server, ftp_user, ftp_pass);


        if (databit == "8bit" && paritybit == "none" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8N1);
        }
        if (databit == "8bit" && paritybit == "even" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8E1);
        }
        if (databit == "8bit" && paritybit == "odd" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8O1);
        }
        if (databit == "8bit" && paritybit == "none" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8N2);
        }
        if (databit == "8bit" && paritybit == "even" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8E2);
        }
        if (databit == "8bit" && paritybit == "odd" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8O2);
        }

        if (databit == "7bit" && paritybit == "none" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7N1);
        }
        if (databit == "7bit" && paritybit == "even" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7E1);
        }
        if (databit == "7bit" && paritybit == "odd" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7O1);
        }
        if (databit == "7bit" && paritybit == "none" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7N2);
        }
        if (databit == "7bit" && paritybit == "even" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7E2);
        }
        if (databit == "7bit" && paritybit == "odd" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7O2);
        }

  slave.start();

  if (initWiFi()) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/index.html", String(), false, processor);
    });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/input", HTTP_GET, [](AsyncWebServerRequest * request) {

      request->send(SPIFFS, "/input.html", String(), false, processor);
    });

    server.on("/jquery", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/jquery.min.js", "text/plain");
    });
        server.on("/bootstrap", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/bootstrap.min.js", "text/plain");
    });
    server.on("/style", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/style.css", "text/plain");
    });

    server.on("/forgetPass", HTTP_GET, [](AsyncWebServerRequest * request) {
      
      request->send(SPIFFS, "/forgetPass.html", String(), false, processor);
      
    });

    server.on("/ChangePassword", HTTP_GET, [](AsyncWebServerRequest * request) {
//      String usernameAndPassword ="";
//      Serial.println(username);
//      Serial.println(password);
//      usernameAndPassword = username +""+ password;
      String newPass = "";
      if(request->hasParam("newpass")){
        newPass = request->getParam("newpass")->value();
        password = newPass;
        writeFile(SPIFFS, passwordPath, password.c_str());
        request->send(200, "text/plain", "ok");
      }
      
      
      
    });
    server.on("/CheckOldPassword", HTTP_GET, [](AsyncWebServerRequest * request) {
      String oldPass = "";
      if(request->hasParam("oldpass")){
        oldPass = request->getParam("oldpass")->value();
      }
      
      if(oldPass == password){
          request->send(200, "text/plain", "pass");
      }else{
        request->send(200, "text/plain", "no");
      }

      
    
      
    });

    server.on("/ValidateUsernameAndPassword", HTTP_GET, [](AsyncWebServerRequest * request) {
      if(request->hasParam("username") && request->hasParam("password")){
        String usernameParam = request->getParam("username")->value();
        String passwordParam = request->getParam("password")->value();
        if(usernameParam == username && passwordParam == password){
          request->send(200, "text/plain", "pass");
        }else{
          request->send(200, "text/plain", "no");
        }
      }
    });

    server.on("/resetpassword", HTTP_GET, [](AsyncWebServerRequest * request) {
     password = "admin";
     writeFile(SPIFFS, passwordPath, password.c_str());
    });
  
    
    server.on("/saveTopic", HTTP_GET, [](AsyncWebServerRequest * request) {
      
      for(int i = 0;i < rowNum;i++){
        String inputRegNum = "inputReg"+ String(i);
        if(request->hasParam(inputRegNum)){
          topics[i + addressNumIdx-1] = request->getParam(inputRegNum)->value();
          
        }
      }
      WriteTopic();
      ReadTopic();
      request->send(200, "text/plain", "ok");
    });

    server.on("/getSavedTopic", HTTP_GET, [](AsyncWebServerRequest * request) {
    String concate = "";
      for(int i = 0;i < 63; i++){
        concate = concate + topics[i] + "\n";
      }
      Serial.print(concate);
//      for(int i = 0;i< 48; i++){
//        Serial.print(topics[i]);
//      }
      request->send(200, "text/plain", concate);
    });
    
    server.on("/getModbusData", HTTP_GET, [](AsyncWebServerRequest * request) {
      //Serial.println(isConnected);
//      if(request->hasParam("addressNum") && request->hasParam("rowIdx")){
//        
//        int addressNum = request->getParam("addressNum")->value().toInt();
//        int rowIdx = request->getParam("rowIdx")->value().toInt();
//      }

//      for(int i = 0;i < rowNum;i++){
//        String inputRegNum = "inputReg"+ String(i);
//        if(request->hasParam(inputRegNum)){
//          topics[i + addressNumIdx-1] = request->getParam(inputRegNum)->value();
//          
//        }
//        
//      }
  
      String modData = "";
      
         for (int i = addressNumIdx-1; i < rowNum+addressNumIdx-1; i++) {

        //Serial.println(i);
        
          if(i+1 < 41){
            int bitValue = (Data[i / 16] >> (i % 16)) & 1;  // อ่านค่า Bit ใน Coil
            modData = modData + bitValue  + "\n";
          }else{
            modData = modData + Data[i] + "\n";
          }
        
        
        }
      
      


//      for(int i = 0; i < 48; i++){
//        modData = modData + topics[i] + "\n";
//      }

      String mqttConnectedString = "";
      
      if(isMQTTConnected){
        mqttConnectedString =  "1";
      }else{
        mqttConnectedString =  "0";
      }

      modData = modData + mqttConnectedString + "\n";
      
      String isconnect = "";
      if (isConnected) {
        isconnect = "1";
      } else {
        isconnect = "0";
      }

      modData = modData + isconnect + "\n";
      //Serial.println(modData);
      //std::string str(Data.begin(), Data.end());
      request->send(200, "text/plain", modData.c_str());
      



    });

     server.on("/checkIP", HTTP_POST, [](AsyncWebServerRequest * request) {

     //String ssidParam = "";
     //String passParam = "";
     String ipParam = "";
     
//     if(request -> hasParam("ssid")){
//      ssidParam = request->getParam("ssid")->value();
//     }
//     if(request -> hasParam("pass")){
//      passParam = request->getParam("pass")->value();
//     }
     if(request -> hasParam("ip")){
      ipParam = request->getParam("ip")->value();
     }
      
       
        Serial.println(ipParam);
        //Serial.println(ssidParam);
        //Serial.println(passParam);
        
        WiFi.begin("", "");
        
        unsigned long currentMillis = millis();
        previousMillis = currentMillis;

        while (WiFi.status() != WL_CONNECTED) {
          currentMillis = millis();
            if (currentMillis - previousMillis >= interval) {
              Serial.println("Failed to connect.");
              break;
            }
        }
      
        bool success = Ping.ping(ipParam.c_str(), 1);
        //Serial.print(success);
        if (success) {
          request->send(200, "text/plain", "IP has already been established. Please select another IP address.");
        }else{
          request->send(200, "text/plain", "Ok!");
        }

    });
    //    server.on("/getConnectedStatus", HTTP_GET, [](AsyncWebServerRequest * request) {
    //    String isconnect ="";
    //    if(isConnected){
    //      isconnect = "1";
    //    }else{
    //      isconnect = "0";
    //    }
    //
    //      request->send(200, "text/plain",isconnect.c_str());
    //    });

    server.on("/set", HTTP_POST, [](AsyncWebServerRequest * request) {
      int paramCount = 8;
//      for (int i = 0; i < paramCount; i++) {
//        if (request->hasParam(masterParamSettings[i])) {
//          Serial.println(request->getParam(masterParamSettings[i])->value());
//        }
//
//      }

      if(request->hasParam("port")){
        port = request->getParam("port")->value();
        writeFile(SPIFFS, portPath, port.c_str());
      }
      
      
      if(request->hasParam("modbus")){
        modbus = request->getParam("modbus")->value();
        writeFile(SPIFFS, modbusPath, modbus.c_str());
      }
      

      if (request->hasParam("timeoutTime")) {
        eventInterval = request->getParam("timeoutTime")->value().toInt();
        writeFile(SPIFFS, eventIntervalPath, String(eventInterval).c_str());
      }
      //Serial.print(eventInterval);


      if (request->hasParam("modbusAddress")) {
        modbusAddress = request->getParam("modbusAddress")->value().toInt();
        Modbus slave(modbusAddress, Serial1, 0);
        writeFile(SPIFFS, modbusAddressPath, String(modbusAddress).c_str());
        //Serial.println(modbusAddress);
      }

      if (request->hasParam("baud")) {
        baudRate = request->getParam("baud")->value().toInt();
        writeFile(SPIFFS, baudRatePath, String(baudRate).c_str());
        //Serial.println(baudRate);
      }
      if (request -> hasParam("databit") && request -> hasParam("stopbit") && request -> hasParam("parity")) {
        String dataBit = request->getParam("databit")->value();
        String stopBit = request->getParam("stopbit")->value();
        String parityBit = request->getParam("parity")->value();
        databit = dataBit;
        stopbit = stopBit;
        paritybit = parityBit;
        writeFile(SPIFFS, databitPath, databit.c_str());
        writeFile(SPIFFS, stopbitPath, stopbit.c_str());
        writeFile(SPIFFS, paritybitPath, paritybit.c_str());

        
        //Serial.print(databit);Serial.print(" ");Serial.print(stopbit);Serial.print(" ");Serial.print(paritybit);

        if (dataBit == "8bit" && parityBit == "none" && stopBit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8N1);
        }
        if (dataBit == "8bit" && parityBit == "even" && stopBit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8E1);
        }
        if (dataBit == "8bit" && parityBit == "odd" && stopBit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8O1);
        }
        if (dataBit == "8bit" && parityBit == "none" && stopBit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8N2);
        }
        if (dataBit == "8bit" && parityBit == "even" && stopBit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8E2);
        }
        if (dataBit == "8bit" && parityBit == "odd" && stopBit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8O2);
        }

        if (dataBit == "7bit" && parityBit == "none" && stopBit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7N1);
        }
        if (dataBit == "7bit" && parityBit == "even" && stopBit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7E1);
        }
        if (dataBit == "7bit" && parityBit == "odd" && stopBit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7O1);
        }
        if (dataBit == "7bit" && parityBit == "none" && stopBit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7N2);
        }
        if (dataBit == "7bit" && parityBit == "even" && stopBit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7E2);
        }
        if (dataBit == "7bit" && parityBit == "odd" && stopBit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7O2);
        }
      slave.start();
      }


      request->send(200, "text/plain", "Ok!");
    });


    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {

      String inputMessage;
      String inputParam = "inputReg";
      String busDataParam = "busData";
      String modbusAddress;
      StaticJsonDocument<5000> doc;

      long wifiStrength = WiFi.RSSI();

      if (request->hasParam("rowNum")){//receiving topic count from web page
        num = request->getParam("rowNum")->value().toInt();
        
//        Data.resize(num);
//        Serial.print(Data.size());
      }
      //      if (request->hasParam("slaveID") && request->getParam("slaveID")->value().length() != 0) { //receiving modbus address from web page
      //        modbusAddress = request->getParam("slaveID")->value();
      //        Serial.println(modbusAddress);
      //      }



      String topic[num];

      concate = "";

      if(addressNumIdx  >= 41){
        doc["rssi"] = wifiStrength;
      }
      
      
      
      for (int i = 0; i < num; i++) {


        if (request->hasParam(inputParam + String(i)) && request->hasParam(busDataParam + String(i))) {
          inputMessage = request->getParam(inputParam + String(i))->value();
          Serial.println(request->getParam(busDataParam + String(i))->value());
          
          doc[inputMessage] = request->getParam(busDataParam + String(i))->value().toInt();

          
          topic[i] = inputMessage;

          String str = topic[i];
          char* char_array = new char[str.length() + 1];
          char_array[str.length()] = '\0';

          for (int i = 0; i < str.length(); i++) {
            char_array[i] = str[i];
            //                concate = concate + char_array[i]+",";
            
            //Serial.print(char_array[i]);
          }
          
          //          concate = concate + "\n";
                            
          Serial.println();
          //Serial.println(topic[i]);
          /*------------------------------------------------------*/
          //INSERT MODBUSMASTER CODE HERE

          //EXAMPLE
          //client.publish(topic[i], //input your data here);
          //client.publish(topic[i], //char_array);



          /*------------------------------------------------------*/
          delete[] char_array;
        }

      }
      String jsonStr;
      serializeJson(doc, jsonStr);
      
       if(client.connect("espClient")){
        request->send(200, "text/plain", "MQTT Connected");
        
        Serial.println("broker connected");
          //client.publish("outTopic", "Hi, I'm ESP32 ^^");
        
          String machineTopic = mqttTopic;
          client.publish(machineTopic.c_str(), jsonStr.c_str());
       }else{
        request->send(200, "text/plain", "MQTT Disconnected");
        Serial.println("broker disconnected");
       }
        
          
        
    
      //Serial.println(jsonStr);

      //      if (request->hasParam("modbus")) {//receive modbus type from web page drop down option (master/slave)
      //
      //        String test = request->getParam("modbus")->value();
      //
      //        if (test == "master") {
      //          Serial.println(test);
      //
      //          for (int i = 0; i < num; i++) {
      //
      //            if (request->hasParam(inputParam + String(i))) {
      //              inputMessage = request->getParam(inputParam + String(i))->value();
      //              topic[i] = inputMessage;
      //
      //              String str = topic[i];
      //              char* char_array = new char[str.length() + 1];
      //              char_array[str.length()] = '\0';
      //
      //              for (int i = 0; i < str.length(); i++) {
      //                char_array[i] = str[i];
      //                Serial.print(char_array[i]);
      //              }
      //
      //              Serial.println();
      //
      //              //Serial.println(topic[i]);
      //              /*------------------------------------------------------*/
      //              //INSERT MODBUSMASTER CODE HERE
      //
      //              //EXAMPLE
      //              //client.publish(topic[i], //input your data here);
      //              //client.publish(topic[i], //char_array);
      //
      //
      //
      //              /*------------------------------------------------------*/
      //              delete[] char_array;
      //            }
      //
      //          }
      //        } else if (test == "slave") {
      //          Serial.println(test);
      //          String topic[num];
      //
      //          for (int i = 0; i < num; i++) {
      //
      //            if (request->hasParam(inputParam + String(i))) {
      //              inputMessage = request->getParam(inputParam + String(i))->value();
      //              topic[i] = inputMessage;
      //              Serial.println(topic[i]);
      //              //INSERT MODBUSSLAVE CODE HERE
      //
      //              //client.publish(topic[i], //input your data here);
      //            }
      //          }
      //        }
      //      }


      request->send(SPIFFS, "/input.html", String(), false, processor);
    });

    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest * request) {


      String ssidParam = request->getParam("ssid")->value();
      String passParam = request->getParam("pass")->value();
      String ipParam = request->getParam("ip")->value();
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ipParam + "<br><br>If connection could not be established or the desired IP address has already been assigned. <br>Please try reconnect to ESP32 and go to IP address 192.168.4.1 again.");
      String gatewayParam = request->getParam("gateway")->value();
      String subnetStringParam = request->getParam("subnetString")->value();
      String hostnameParam = request->getParam("hostname")->value();
//      String mqttAddressParam = request->getParam("mqttserver")->value();
//      String mqttPortParam = request->getParam("mqttport")->value();

      //      String dhcpcheckedParam = "";
      //      if (request->hasParam("dhcpchecked")) {
      //        dhcpcheckedParam = "true";
      //      } else {
      //        dhcpcheckedParam = "false";
      //      }

      //      request->getParam("dhcpchecked")->value();

      writeFile(SPIFFS, ssidPath, ssidParam.c_str());
      writeFile(SPIFFS, passPath, passParam.c_str());
      writeFile(SPIFFS, ipPath, ipParam.c_str());
      writeFile(SPIFFS, gatewayPath, gatewayParam.c_str());
      writeFile(SPIFFS, subnetStringPath, subnetStringParam.c_str());
      writeFile(SPIFFS, hostnamePath, hostnameParam.c_str());
//      writeFile(SPIFFS, mqttAddressPath, mqttAddressParam.c_str());
//      writeFile(SPIFFS, mqttPortPath, mqttPortParam.c_str());
      //      writeFile(SPIFFS, dhcpcheckedPath, dhcpcheckedParam.c_str());
      //request->send(SPIFFS, "/wifimanager.html", "text/html");
      


      
      WiFi.setHostname(hostnameParam.c_str());
      WiFi.begin(ssidParam.c_str(), passParam.c_str());
      unsigned long currentMillis = millis();
      previousMillis = currentMillis;

      while (WiFi.status() != WL_CONNECTED) {
        currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
          Serial.println("Failed to connect.");
          isUsed = false;
          writeFile(SPIFFS, ssidPath, NULL);
          //ip = "";
          writeFile(SPIFFS, ipPath, NULL);
          dhcpchecked = "false";
          writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());
          break;
        }
      }
      //      if (dhcpchecked != "true" && ip != ipParam) {
      bool success = Ping.ping(ipParam.c_str(), 1);
      Serial.print(success);
      if (success) {
        //ssid = "";
        writeFile(SPIFFS, ssidPath, NULL);
        //ip = "";
        writeFile(SPIFFS, ipPath, NULL);
        isUsed = true;
        request->send(200, "text/plain", "IP has already been established. ESP will restart, connect to ESP32 Access Point and go to IP address: 192.168.4.1");
        
      } else {
        
        dhcpchecked = "false";
        writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());
      }
      //      }


      delay(100);
      ESP.restart();
    });

    server.on("/resetESP32", HTTP_POST, [](AsyncWebServerRequest * request) {
      //ssid = "";
      writeFile(SPIFFS, ssidPath, NULL);
      //ip = "";
      writeFile(SPIFFS, ipPath, NULL);

      dhcpchecked = "false";
      writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());

      request->send(200, "text/plain", "Done. ESP will restart, connect to ESP32 Access Point and go to IP address: 192.168.4.1");
      delay(100);
      ESP.restart();
    });

    server.on("/setModbusQuantity", HTTP_POST, [](AsyncWebServerRequest * request) {
      if (request->hasParam("rowNum") && request->hasParam("addressNumIdx")) {//receiving topic count from web page
          rowNum = request->getParam("rowNum")->value().toInt();
          addressNumIdx = request->getParam("addressNumIdx")->value().toInt();
        


          
//        num = request->getParam("rowNum")->value().toInt();
//        int addressNumIdx = request->getParam("addressNumIdx")->value().toInt();
//        
//        Data.resize(num+addressNumIdx);
//        Serial.print(Data.size());
      }
    });

    server.on("/getHostname", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(200, "text/plain", hostname.c_str());
    });
    server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest * request){
      String concate = "";
      concate = concate + port + "\n";
      concate = concate + modbus + "\n";
      concate = concate + String(baudRate) + "\n";
      concate = concate + databit+ "\n";
      concate = concate + stopbit + "\n";
      concate = concate + paritybit + "\n";
      concate = concate + eventInterval + "\n";
      concate = concate + String(modbusAddress) + "\n";
      request->send(200, "text/plain", concate);
    });

    server.on("/getMQTTSettings", HTTP_GET, [](AsyncWebServerRequest * request){
      String concate = "";
      concate = concate + mqttTopic + "\n";
      concate = concate + mqttAddress + "\n";
      concate = concate + String(mqttPort) + "\n";
      concate = concate + String(mqttInterval) + "\n";
      concate = concate + mqttEnable + "\n";
      Serial.println(concate);
      request->send(200, "text/plain", concate);
    });


    server.on("/setMQTTSettings", HTTP_GET, [](AsyncWebServerRequest * request){

      if(request->hasParam("mqttaddress") && request->hasParam("mqttport") && request->hasParam("mqttinterval") && request->hasParam("mqttenable") && request->hasParam("mqtttopic") ){
        mqttTopic = request->getParam("mqtttopic")->value();
        mqttAddress = request->getParam("mqttaddress")->value();
        mqttPort = request->getParam("mqttport")->value().toInt();
        mqttInterval = request->getParam("mqttinterval")->value().toInt();
        mqttEnable = request->getParam("mqttenable")->value();

        writeFile(SPIFFS, mqttTopicPath, mqttTopic.c_str());
        writeFile(SPIFFS, mqttAddressPath, mqttAddress.c_str());
        writeFile(SPIFFS, mqttPortPath, String(mqttPort).c_str());
        writeFile(SPIFFS, mqttIntervalPath, String(mqttInterval).c_str());
        writeFile(SPIFFS, mqttEnablePath, mqttEnable.c_str());
      
        //Serial.println(mqttAddress + mqttPort + mqttInterval + mqttEnable);
        
        client.setCallback(callback);
        
        client.setServer(mqttAddress.c_str(), mqttPort);
        
         
         
        if (databit == "8bit" && paritybit == "none" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8N1);
        }
        if (databit == "8bit" && paritybit == "even" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8E1);
        }
        if (databit == "8bit" && paritybit == "odd" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_8O1);
        }
        if (databit == "8bit" && paritybit == "none" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8N2);
        }
        if (databit == "8bit" && paritybit == "even" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8E2);
        }
        if (databit == "8bit" && paritybit == "odd" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_8O2);
        }

        if (databit == "7bit" && paritybit == "none" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7N1);
        }
        if (databit == "7bit" && paritybit == "even" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7E1);
        }
        if (databit == "7bit" && paritybit == "odd" && stopbit == "1bit" ) {
          Serial1.begin(baudRate, SERIAL_7O1);
        }
        if (databit == "7bit" && paritybit == "none" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7N2);
        }
        if (databit == "7bit" && paritybit == "even" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7E2);
        }
        if (databit == "7bit" && paritybit == "odd" && stopbit == "2bit" ) {
          Serial1.begin(baudRate, SERIAL_7O2);
        }
      }

      request->send(200, "text/plain", "OK");
    });


    server.on("/getMQTTStatus", HTTP_GET, [](AsyncWebServerRequest * request){
      if(isMQTTConnected){
      request->send(200, "text/plain", "1");
      }else{
      request->send(200, "text/plain", "0");
      }
    });

    server.on("/getWiFiSettings", HTTP_GET, [](AsyncWebServerRequest * request){
      String concate = "";
      concate = ssid + "\n";
      concate = concate + pass + "\n";
      concate = concate + hostname + "\n";
      concate = concate + ip + "\n";
      concate = concate + gateway + "\n";
      concate = concate + subnetString + "\n";
      request->send(200, "text/plain", concate);
      
    });

    server.on("/getFTPSettings", HTTP_GET, [](AsyncWebServerRequest * request){
      String concate = "";
      concate = ftp_server + "\n";
      concate = concate + ftp_user + "\n";
      concate = concate + ftp_pass + "\n";
      concate = concate + ftp_mcstatus_filename + "\n";
      concate = concate + ftp_mcstatus_directory + "\n";
      concate = concate + ftp_mc_alarmlist_filename + "\n";
      concate = concate + ftp_mc_alarmlist_directory + "\n";
      request->send(200, "text/plain", concate);
      
    });
    
    server.on("/setFTPSettings", HTTP_GET, [](AsyncWebServerRequest * request){
      if(request->hasParam("ftpaddress") 
      && request->hasParam("ftpname") 
      && request->hasParam("ftppass") 
      && request->hasParam("mcstatus_filename") 
      && request->hasParam("mcstatus_path") 
      && request->hasParam("mc_alarmlist_filename") 
      && request->hasParam("mc_alarmlist_path") ){

        ftp_server = request->getParam("ftpaddress")->value();
        ftp_user = request->getParam("ftpname")->value();
        ftp_pass = request->getParam("ftppass")->value();
        ftp_mcstatus_filename = request->getParam("mcstatus_filename")->value();
        ftp_mcstatus_directory = request->getParam("mcstatus_path")->value();
        ftp_mc_alarmlist_filename = request->getParam("mc_alarmlist_filename")->value();
        ftp_mc_alarmlist_directory = request->getParam("mc_alarmlist_path")->value();

        Serial.println(ftp_mcstatus_filename.c_str());
        Serial.println(ftp_mcstatus_directory.c_str());
        Serial.println(ftp_mc_alarmlist_filename.c_str());
        Serial.println(ftp_mc_alarmlist_directory.c_str());

        
        writeFile(SPIFFS, ftp_server_path , ftp_server.c_str());
        writeFile(SPIFFS, ftp_user_path  , ftp_user.c_str());
        writeFile(SPIFFS, ftp_pass_path  , ftp_pass.c_str());
        writeFile(SPIFFS, ftp_mcstatus_filename_path  , ftp_mcstatus_filename.c_str());
        writeFile(SPIFFS, ftp_mcstatus_directory_path  , ftp_mcstatus_directory.c_str());
        writeFile(SPIFFS, ftp_mc_alarmlist_filename_path  , ftp_mc_alarmlist_filename.c_str());
        writeFile(SPIFFS, ftp_mc_alarmlist_directory_path  , ftp_mc_alarmlist_directory.c_str());

        SetupFTP(ftp_server, ftp_user, ftp_pass);
      }

      request->send(200, "text/plain", "OK");
    });
    
    
  } else {

    // Connect to Wi-Fi network with SSID and password
    Serial.print("Setting AP (Access Point)…");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("Original AP IP address: ");
    Serial.println(IP);
    WiFi.mode(WIFI_AP);
    // Remove the password parameter, if you want the AP (Access Point) to be open
    String wifiMacString = WiFi.macAddress();
    wifiMacString = encrypt(wifiMacString);
    String ssidFirstStart = "SMM_" + wifiMacString;
    
    WiFi.softAP(ssidFirstStart);
    Serial.println("Set softAPConfig");
    IPAddress Ip(192, 168, 4, 1);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(Ip, Ip, NMask);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("New AP IP address: ");
    Serial.println(myIP);
    // Print ESP32 Local IP Address
    Serial.println(WiFi.localIP());

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/setupnetworklogin.html", "text/html");
    });

    server.on("/input", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });

       server.on("/restartESP32", HTTP_POST, [](AsyncWebServerRequest * request) {
       WiFi.softAPdisconnect(true);
      ESP.restart();
      
    });
    

    server.serveStatic("/", SPIFFS, "/");

    server.on("/setIP", HTTP_POST, [](AsyncWebServerRequest * request) {
      String ssidParam = request->getParam("ssid")->value();
      String passParam = request->getParam("pass")->value();
      String ipParam = request->getParam("ip")->value();
      String hostnameParam = request->getParam("hostname")->value();
//      String mqttAddressParam = request->getParam("mqttserver")->value();
//      String mqttPortParam = request->getParam("mqttport")->value();
      
      writeFile(SPIFFS, ssidPath, ssidParam.c_str());
      writeFile(SPIFFS, passPath, passParam.c_str());
      writeFile(SPIFFS, ipPath, ipParam.c_str());
      writeFile(SPIFFS, hostnamePath, hostnameParam.c_str());
//      writeFile(SPIFFS, mqttAddressPath, mqttAddressParam.c_str());
//      writeFile(SPIFFS, mqttPortPath, mqttPortParam.c_str());
      
      unsigned long currentMillis = millis();
      previousMillis = currentMillis;

          
        request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ipParam + "<br><br>If connection could not be established or the desired IP address has already been assigned. <br>Please try reconnect to ESP32 and go to IP address 192.168.4.1 again.");
        delay(100);
        WiFi.softAPdisconnect(true);
        delay(100);
        ESP.restart();
        Serial.println("RESTARTING");
       
  
      
      
      
//      request->send_P(200, "text/html", index_html ,processor);

    });
    
    server.on("/checkIP", HTTP_POST, [](AsyncWebServerRequest * request) {

        String hostname = "";
        String ssid = "";
        String pass = "";
//        String mqttserver ="";
//        int mqttport = 0;
        
        if(request->hasParam("hostname") && request->hasParam("ssid") && request->hasParam("pass")){
          hostname = request->getParam("hostname")->value();
          ssid = request->getParam("ssid")->value();
          pass = request->getParam("pass")->value();
//          mqttserver = request->getParam("mqttserver")->value();
//          mqttport = request->getParam("mqttport")->value().toInt();
          
          if(hostname == "" || ssid == "" || pass == ""){
            request->send(200, "text/plain", "Please put input correctly.");
          }else{

          unsigned long currentMillis = millis();
          previousMillis = currentMillis;

//          client.setCallback(callback);
//          client.setServer(mqttserver.c_str(), mqttport);

          
            
          WiFi.setHostname(hostname.c_str());
          WiFi.begin(ssid.c_str(), pass.c_str());
        


          while (WiFi.status() != WL_CONNECTED) {
            currentMillis = millis();


            if (currentMillis - previousMillis >= interval) {
              Serial.println("Failed to connect.");
              break;
            }
          }
           if(WiFi.status() == WL_CONNECTED){
//            if(client.connect("espClient")){
//               request->send(200, "text/plain", WiFi.localIP().toString() + "OK");
//            }else{
//               request->send(200, "text/plain", "Could not connect to the corresponding MQTT Server.");
//            }

            request->send(200, "text/plain", WiFi.localIP().toString() + "OK");
            
           
           }else{
            request->send(200, "text/plain", "Unable to connect. Please try again.");
           }

          }
       
        }else{
          request->send(200, "text/plain", "Please put input correctly.");
        }


    });


    server.on("/", HTTP_POST, [](AsyncWebServerRequest * request) {
      int params = request->params();
      String ipParam = "";
      for (int i = 0; i < params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        Serial.println(p->name()); Serial.println(p->value ());


        if (p->isPost()) {
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ipParam = p->value();
            //            if (p->value() == ip) {
            //              return;
            //            }

            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(SPIFFS, gatewayPath, gateway.c_str());
          }
          if (p->name() == PARAM_INPUT_5) {
            subnetString = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(subnetString);
            writeFile(SPIFFS, subnetStringPath, subnetString.c_str());
          }
          if (p->name() == PARAM_INPUT_6) {
            hostname = p->value().c_str();
            Serial.print("hostname set to: ");
            Serial.println(hostname);
            writeFile(SPIFFS, hostnamePath, hostname.c_str());
          }

          if (p->name() == "dhcpchecked") {
            if (p->value() == "true") {
              dhcpchecked = "true";
              writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());
            }
          }
          if (p->name() == "dhcpunchecked") {
            if (p->value() == "true") {
              dhcpchecked = "false";
              writeFile(SPIFFS, dhcpcheckedPath, dhcpchecked.c_str());
            }
          }


          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      dhcpchecked = "true";
      
      writeFile(SPIFFS, ipPath, WiFi.localIP().toString().c_str());
     Serial.print("IP Address of Current WiFi: "); Serial.println(WiFi.localIP());

      if (dhcpchecked == "false") {
        request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip + "\n\nIf connection could not be established or the desired IP address has already been assigned. \nPlease try reconnect to ESP32 and go to IP address 192.168.4.1 again.");
        WiFi.setHostname(hostname.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        unsigned long currentMillis = millis();
        previousMillis = currentMillis;

        while (WiFi.status() != WL_CONNECTED) {
          currentMillis = millis();


          if (currentMillis - previousMillis >= interval) {
            Serial.println("Failed to connect.");
            isUsed = false;
            break;
          }
        }


        if (dhcpchecked != "true" && ip != ipParam) {
          bool success = Ping.ping(ip.c_str(), 1);
          Serial.print(success);
          if (success) {
            //ssid = "";
            writeFile(SPIFFS, ssidPath, NULL);
            //ip = "";
            writeFile(SPIFFS, ipPath, NULL);
            isUsed = true;
          }
        }


        delay(100);
        ESP.restart();
      }
      if (dhcpchecked == "true") {

        WiFi.setHostname(hostname.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        unsigned long currentMillis = millis();
        previousMillis = currentMillis;

        while (WiFi.status() != WL_CONNECTED) {
          currentMillis = millis();


          if (currentMillis - previousMillis >= interval) {
            Serial.println("Failed to connect.");
            break;
          }
        }
          //String dhcpip = WiFi.localIP().toString();
            //writeFile(SPIFFS, ipPath, dhcpip.c_str());
            
//        Serial.println(WiFi.localIP());
        
        request->send_P(200, "text/html", index_html ,processor);
          
        //delay(100);
        //ESP.restart();



      }
      //Serial.println(WiFi.localIP());


    });

  }



  // Start server

     
 /*   while (!client.connected()) {

        if (client.connect("espClient")) {
            Serial.println("broker connected");
            client.publish("outTopic", "Hi, I'm ESP32 ^^");
            
        } else {
            Serial.print("failed with state ");
            
        }
    }*/
    
  server.onNotFound(notFound);
  server.begin();
  client.setCallback(callback);
  client.setServer(mqttAddress.c_str(), mqttPort);
  //Serial.println(Ping.ping(ip.c_str(), 1));

}

//int8_t state = 0;
//
//
//const int modNum = 22;
//
//uint16_t Data[num];



int prevState;

bool hasData;

int readCount;

int period = 20000;
unsigned long last_time = 0;


String prev_mc_status_state = "";
String prev_alarm_list_state = "";

String saved_alarm_list_data[30] = {
  "","","","","","","","","","",
  "","","","","","","","","","",
  "","","","","","","","","","",
};


unsigned long last_time_mqtt = 0;
bool prevIsMQTTConnected;


int oldWiFiStatus;


unsigned long previousConnectedMillis = 0;
const long connectedinterval = 1000;

//String mc_status_data;

String old_mc_status_data;

void loop() {

  unsigned long currentConnectedMillis = millis();
  if(currentConnectedMillis - previousConnectedMillis >= connectedinterval){
      previousConnectedMillis = currentConnectedMillis;
      if(prevIsMQTTConnected == false && isMQTTConnected == false){
       //client.setCallback(callback); 
       client.setServer(mqttAddress.c_str(), mqttPort);
      }
  
     if(WiFi.status() != WL_CONNECTED || !isMQTTConnected){
          digitalWrite(led_connection, LOW);
          delay(100);
          digitalWrite(led_connection, HIGH);
    }
  }
  
  if(prevIsMQTTConnected != isMQTTConnected){
    if(prevIsMQTTConnected && !isMQTTConnected){
      client.setServer("",mqttPort);
    }
    if(!prevIsMQTTConnected && isMQTTConnected && WiFi.status() == WL_CONNECTED){
      digitalWrite(led_connection, HIGH);
    }

    prevIsMQTTConnected = isMQTTConnected;
  }

  
   int8_t state = slave.poll( &Data[0] , Data.size() );
  //Serial.println(state);

  unsigned long currentTime2 = millis();

   /*if(oldWiFiStatus != WiFi.status()){
      if(oldWiFiStatus != WL_CONNECTED && WiFi.status() == WL_CONNECTED){
        ftp.OpenConnection();
        
      }
     if(oldWiFiStatus == WL_CONNECTED && WiFi.status() != WL_CONNECTED){
        ftp.CloseConnection();
     }
      
      oldWiFiStatus = WiFi.status();
    }*/
    
 /*if(currentTime2 - last_time >= period){
    last_time = millis();


    
    if(WiFi.status() == WL_CONNECTED){
      
      if(ftp_mcstatus_filename!="" && ftp_mc_alarmlist_filename!=""){
        ftp.OpenConnection();
        sent_ftp_mc_status();
        sent_ftp_alarm_list();
        ftp.CloseConnection();
      }

  
      
      //ftp.InitFile("Type A");
      //ftp.NewFile("hello_world.txt");
      //ftp.Write("Hello World");


    }
    
    //sent_ftp_mc_status();
    
    //readAppendedFile(SPIFFS, "/mc_status.txt");
    //readAppendedFile(SPIFFS, "/alarm_list.txt");
  }*/
  unsigned long currentTimeMQTT = millis();

   if(client.connect("espClient")){
    

    
    isMQTTConnected = true;

    if(mqttEnable == "true"){
      
      if(currentTimeMQTT - last_time_mqtt >= mqttInterval){
        last_time_mqtt = millis();

        digitalWrite(led_run, LOW);
        delay(100);
        digitalWrite(led_run, HIGH);
        
        StaticJsonDocument<5000> doc;
        
        long wifiStrength = WiFi.RSSI();
        
         bool isNotEmpty = false;
         
        for(int i = 40 ; i < 63; i++){
         
          if(topics[i] != ""){
            isNotEmpty = true;
            break;
          }
        }
        
        if(isNotEmpty == true){
          doc["rssi"] = wifiStrength;
          
        }
        
       
        
        for(int i = 40;i < 63;i++){
        switch(i){
          case 40:
            if(!(Data[i] >= 2023)){
              continue;            
            }
          break;
          case 41:
          if(!(Data[i] > 0 && Data[i] <= 12)){
            continue;
          }
          break;
          case 42:
          if(!(Data[i] > 0 && Data[i] <= 31)){
            continue;
          }
          break;
          case 43:
          if(!(Data[43] >= 0 && Data[43] <= 23)){
            continue;
          }
          break;
          case 44:
          if(!(Data[44] >= 0 && Data[44] <= 59)){
            continue;
          }
          break;
          case 45:
          if(!(Data[45] > 0 && Data[45] <= 59)){
            continue;
          }
          break;
        }
        
        /*if(Data[40] >= 2023
        && Data[41] > 0 && Data[41] <= 12
        && Data[42] > 0 && Data[42] <= 31
        && Data[43] >= 0 && Data[43] <= 23
        && Data[44] >= 0 && Data[44] <= 59
        && Data[45] > 0 && Data[45] <= 59){
          
        }*/
          if(topics[i] != ""){
            doc[topics[i]] = Data[i];
          }
            
        }
        if(isNotEmpty == true){
          String jsonStr;
          serializeJson(doc, jsonStr);
          String machineTopic = mqttTopic;
          client.publish(machineTopic.c_str(), jsonStr.c_str());
        }
        
        }

    }
    

   }else{
    isMQTTConnected = false;

   }
  





  
  unsigned long currentTime = millis();

  //  Serial.println(isConnected);

  //AsyncWebServer.handleClient()



  if (prevState != state) {
//    Serial.println(state);

    if (prevState == 0 && state >= 6) {
    
      //       Serial.println(state);
      //currentMillis = millis();
      isConnected = true;
      previousTime = currentTime;


       String tempData = "";
       for(int i = 0; i < 41; i++){
        int bitValue = (Data[i / 16] >> (i % 16)) & 1;  // อ่านค่า Bit ใน Coil
        tempData = tempData + String(bitValue);
      }
        String mc_status_state = "";
        String alarm_list_state = "";

        for(int i = 0;i < 30; i++){//0 to 29 is a data that holds mc alarm list state specifically.
          alarm_list_state = alarm_list_state + tempData.charAt(i);
        }
        
        for(int i = 30; i < 40;i++){//do the same as above except it iterates through 30 - 39 with mc status state
          mc_status_state = mc_status_state + tempData.charAt(i);
        }

        if(prev_alarm_list_state == ""){//replace prev alarm list with current status on initial start up
          prev_alarm_list_state = alarm_list_state;
        }
        
        if(prev_mc_status_state == ""){//replace prev status with current status on initial start up
          prev_mc_status_state = mc_status_state;
        }
        
        if(prev_mc_status_state != mc_status_state){//mc status process
          
          Write_MC_Status();
          if(prev_mc_status_state == "0000000001" && mc_status_state == "1000000001"){

            String mc_status_data = "\r\n" + formatTime(Data[40], Data[41], Data[42], Data[43], Data[44], Data[45]) + ",";
               
//               Serial.print(formatTime(Data[40], Data[41], Data[42], Data[43], Data[44], Data[45]));
//               Serial.print(",");
               
             int statusCount = 1;
             for(int i = 0;i<mc_status_state.length();i++){
                
              if(mc_status_state.charAt(i) == '1'){
                 break; 
              }
                statusCount++;
              }
//               Serial.println(statusCount);
              mc_status_data = mc_status_data + statusCount;
               
              int lines = countLines("/mc_status.txt");
              if(lines > 100){

                String mc_status_content = readFileWithLine(SPIFFS, "/mc_status.txt");
                      
                String concate_final = "occurred,mc_status";
                String concate_old = "";
                for(int i = 1;i <= 100-1;i++){
                  concate_old = concate_old + "\n" + getValue(mc_status_content, '\n', i);
                }
                concate_final = concate_final + /*mc_status_data has \n in front*/mc_status_data + concate_old;

                writeFile(SPIFFS, "/mc_status.txt", concate_final.c_str());

            }else{
              if(lines > 1){
                appendFile(SPIFFS, "/mc_status.txt", mc_status_data.c_str());
                String mc_status_content = readFileWithLine(SPIFFS, "/mc_status.txt");
                      
                String concate_final = "occurred,mc_status";
                String concate_old = "";
                for(int i = 1;i <= lines-1;i++){
                  concate_old = concate_old + "\n" + getValue(mc_status_content, '\n', i);
                }
                concate_final = concate_final + /*mc_status_data has \n in front*/mc_status_data + concate_old;

                writeFile(SPIFFS, "/mc_status.txt", concate_final.c_str());

                }else{
                  appendFile(SPIFFS, "/mc_status.txt", mc_status_data.c_str());
              }
                      
            }
            
        }
          //Serial.println(mc_status_state);
          if(mc_status_state != "0000000000" && mc_status_state != "1000000001" && prev_mc_status_state != "1000000001"){//mc status won't do anything if it's 0000000000
               Serial.print(prev_mc_status_state);Serial.print(" : ");Serial.println(mc_status_state);

             String mc_status_data = "\r\n" + formatTime(Data[40], Data[41], Data[42], Data[43], Data[44], Data[45]) + ",";
               
//               Serial.print(formatTime(Data[40], Data[41], Data[42], Data[43], Data[44], Data[45]));
//               Serial.print(",");
               
               int statusCount = 1;
               for(int i = 0;i<mc_status_state.length();i++){
                
                if(mc_status_state.charAt(i) == '1'){
                 break; 
                }
                statusCount++;
               }
//               Serial.println(statusCount);
               mc_status_data = mc_status_data + statusCount;
               
               if(formatTime(Data[40], Data[41], Data[42], Data[43], Data[44], Data[45]) != ""){
                
                Serial.println(mc_status_data);
                  if(Data[40] >= 2023
                    && Data[41] > 0 && Data[41] <= 12
                    && Data[42] > 0 && Data[42] <= 31
                    && Data[43] >= 0 && Data[43] <= 23
                    && Data[44] >= 0 && Data[44] <= 59
                    && Data[45] > 0 && Data[45] <= 59){
                    int lines = countLines("/mc_status.txt");
                    if(lines > 100){

                      String mc_status_content = readFileWithLine(SPIFFS, "/mc_status.txt");
                      
                      String concate_final = "occurred,mc_status";
                      String concate_old = "";
                      for(int i = 1;i <= 100-1;i++){
                          concate_old = concate_old + "\n" + getValue(mc_status_content, '\n', i);
                      }
                      concate_final = concate_final + /*mc_status_data has \n in front*/mc_status_data + concate_old;

                      writeFile(SPIFFS, "/mc_status.txt", concate_final.c_str());

                    }else{
                      if(lines > 1){
                        appendFile(SPIFFS, "/mc_status.txt", mc_status_data.c_str());
                        String mc_status_content = readFileWithLine(SPIFFS, "/mc_status.txt");
                      
                        String concate_final = "occurred,mc_status";
                        String concate_old = "";
                        for(int i = 1;i <= lines-1;i++){
                          concate_old = concate_old + "\n" + getValue(mc_status_content, '\n', i);
                        }
                        concate_final = concate_final + /*mc_status_data has \n in front*/mc_status_data + concate_old;

                        writeFile(SPIFFS, "/mc_status.txt", concate_final.c_str());

                     }else{
                        appendFile(SPIFFS, "/mc_status.txt", mc_status_data.c_str());
                     }
                      
                    }
                      
                  }
                  
               }
                
                //Serial.print("Current line count = ");Serial.println(countLines("/mc_status.txt"));
          }
          //old_mc_status_data = mc_status_data;
          prev_mc_status_state = mc_status_state;
        }



           for(int i = 0; i < 30; i++){//check each alarm list state

            if(prev_alarm_list_state.charAt(i) != alarm_list_state.charAt(i)){
              
              Write_Alarm_List();
              
              if(prev_alarm_list_state.charAt(i) == '0' && alarm_list_state.charAt(i) == '1'){
                //ReadTopic();
                if(topics[i] != ""){
                  saved_alarm_list_data[i] = "\r\n"+ topics[i]+ ",";
                  saved_alarm_list_data[i] = saved_alarm_list_data[i] + formatTime(Data[40], Data[41], Data[42], Data[43], Data[44], Data[45]) + ",";
                }
         
//                Serial.println(saved_alarm_list_data[i]);


              }else{
       
                if(topics[i] != "" && saved_alarm_list_data[i] != ""){
                 saved_alarm_list_data[i] = saved_alarm_list_data[i] + formatTime(Data[40],Data[41],Data[42],Data[43],Data[44], Data[45]);

                  if(formatTime(Data[40],Data[41],Data[42],Data[43],Data[44], Data[45]) != "" 
                  && getValue(saved_alarm_list_data[i],',',1) != "0000-00-00 00:00:00" 
                  && getValue(saved_alarm_list_data[i],',',2) != "0000-00-00 00:00:00"){
                    if(Data[40] >= 2023
                    && Data[41] > 0 && Data[41] <= 12
                    && Data[42] > 0 && Data[42] <= 31
                    && Data[43] >= 0 && Data[43] <= 23
                    && Data[44] >= 0 && Data[44] <= 59
                    && Data[45] > 0 && Data[45] <= 59){
                      //appendFile(SPIFFS, "/alarm_list.txt", saved_alarm_list_data[i].c_str());
                      int lines = countLines("/alarm_list.txt");
                      if(lines > 100){
                        String mc_alarmlist_content = readFileWithLine(SPIFFS, "/alarm_list.txt");
                      
                        String concate_final = "topic,occurred,restored";
                        String concate_old = "";
                        for(int i = 1;i <= 100-1;i++){
                          concate_old = concate_old + "\n" + getValue(mc_alarmlist_content, '\n', i);
                        }
                        concate_final = concate_final + /*mc_status_data has \n in front*/saved_alarm_list_data[i] + concate_old;
//                        Serial.println(concate_final);
                        writeFile(SPIFFS, "/alarm_list.txt", concate_final.c_str());

                        ClearAlarmlistData();

//                        String newLine = "topic,occurred,restored"+saved_alarm_list_data[i];
//                        writeFile(SPIFFS, "/alarm_list.txt", newLine.c_str());
//                        ClearAlarmlistData();
                      
                      }else{
                        if(lines > 1){
                          appendFile(SPIFFS, "/alarm_list.txt", saved_alarm_list_data[i].c_str());
                          String mc_alarmlist_content = readFileWithLine(SPIFFS, "/alarm_list.txt");
                      
                          String concate_final = "topic,occurred,restored";
                          String concate_old = "";
                          for(int i = 1;i <= lines-1;i++){
                            concate_old = concate_old + "\n" + getValue(mc_alarmlist_content, '\n', i);
                          }
                          concate_final = concate_final + /*mc_status_data has \n in front*/saved_alarm_list_data[i] + concate_old;
//                        Serial.println(concate_final);
                          writeFile(SPIFFS, "/alarm_list.txt", concate_final.c_str());

                          ClearAlarmlistData();
                        
                        }else{
                          appendFile(SPIFFS, "/alarm_list.txt", saved_alarm_list_data[i].c_str());
                        }
                     
                          //appendFile(SPIFFS, "/alarm_list.txt", saved_alarm_list_data[i].c_str());
                      }
                    }
                  }

        
                //Serial.println(saved_alarm_list_data[i]);
                //Serial.print("Current line count = ");Serial.println(countLines("/alarm_list.txt"));
                
                }

              }
              
              prev_alarm_list_state[i] = alarm_list_state[i];
            }
            
          }




    }

    prevState = state;
  }

  if (isConnected) {
    //    prevCount--;
    //    if (prevCount <= 0) {
    //      prevCount = intervalCount;
    //      isConnected = false;
    //    }


    if (currentTime - previousTime >= eventInterval) {
      /* Event code */
      isConnected = false;
      /* Update the timing for the next time around */
      previousTime = currentTime;
    }

  }

  if(currentTime2 - last_time >= period){
    last_time = millis();


    
    if(WiFi.status() == WL_CONNECTED){
      
      if(ftp_mcstatus_filename!="" && ftp_mc_alarmlist_filename!=""){
        ftp.OpenConnection();
        sent_ftp_mc_status();
        sent_ftp_alarm_list();
        ftp.CloseConnection();
      }

  
      
      //ftp.InitFile("Type A");
      //ftp.NewFile("hello_world.txt");
      //ftp.Write("Hello World");


    }
    
    //sent_ftp_mc_status();
    
    //readAppendedFile(SPIFFS, "/mc_status.txt");
    //readAppendedFile(SPIFFS, "/alarm_list.txt");
  }

}
// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}

String encrypt(String msg)
{ 
    String ret = "";
    int gennum;
    for(int i = 0; i < msg.length();i++){
      gennum = gennum + int(msg.charAt(i));
    }
    ret = String(gennum*1000, HEX);
    ret.toUpperCase();
    return ret;
}
void readAppendedFile(fs::FS &fs, const char * path){
//    Serial.printf("\r\nReading file: %s\r\n", path);
//
//    File file = fs.open(path);
//    if(!file || file.isDirectory()){
//        //Serial.println("- failed to open file for reading");
//        return;
//    }
//
//    //Serial.println("- read from file:");
//    while(file.available()){
//        Serial.write(file.read());
//    }
//    file.close();
}


String formatTime(uint16_t data0, uint16_t data1, uint16_t data2, uint16_t data3, uint16_t data4, uint16_t data5) {

  if(data0 < 2023 && data0 > 0){data0 = data0+2000;}
  String formatted_time = String(data0) + "-" + (data1 < 10 ? "0" : "") + String(data1) + "-" + (data2 < 10 ? "0" : "") + String(data2) + " ";

  String hour = (data3 < 10) ? "0" + String(data3) : String(data3);
  String minute = (data4 < 10) ? "0" + String(data4) : String(data4);
  String second = (data5 < 10) ? "0" + String(data5) : String(data5);

  formatted_time += hour + ":" + minute + ":" + second;

  return formatted_time;
}
int countLines(const char *path){
  File file = SPIFFS.open(path, "r");
  if(!file){
    Serial.println("Failed to open file for reading");
    return 0;
  }
  int lineCount = 0;
  while(file.available()){
    char c = file.read();
    if(c == '\n'){
      lineCount++;
    }
  }
  file.close();
  return lineCount+1;
}
void sent_ftp_mc_status(){
//  int lineCount = countLines("/mc_status.txt");
//  if(lineCount >= 2){
//    ftp.OpenConnection();
//    File dataFile = SPIFFS.open("/mc_status.txt","r");
//    if(dataFile){
//      String fileContent = dataFile.readString();
//      dataFile.close();
//      if(fileContent.length() > 0){
//        ftp.InitFile("Type A");
//        ftp.ChangeWorkDir("/data_mcstatus/gd/");
//        String machinestatusname = "gd_mc_status_"+hostname+".txt";
//        ftp.NewFile(machinestatusname.c_str());
//        ftp.Write(fileContent.c_str());
//        ftp.CloseFile();
//      }
//    }else{
//      Serial.println("Failed to open mc_status.txt for reading");
//    }
//    ftp.CloseConnection();
//  }else{
//    String test = "gd_mcstatus_"+hostname+".txt";
//    Serial.print("Failed to open ");Serial.print(test);Serial.println("Empty");
//  }



      File dataFile = SPIFFS.open("/mc_status.txt","r");
      if(dataFile){
        String fileContent = dataFile.readString();
        dataFile.close();
        ftp.InitFile("Type A");
        //ftp.ChangeWorkDir("/data_mcstatus");
        String workDir = "/data_mcstatus/"+ftp_mcstatus_directory;
        ftp.ChangeWorkDir(workDir.c_str());
        String filename_mcstatus = ftp_mcstatus_filename + ".txt";
        ftp.NewFile(filename_mcstatus.c_str());
        ftp.Write(fileContent.c_str());
        ftp.CloseFile();
      }


}

void sent_ftp_alarm_list(){

    
      File dataFile = SPIFFS.open("/alarm_list.txt","r");
      if(dataFile){
        String fileContent = dataFile.readString();
        dataFile.close();
        ftp.InitFile("Type A");
        //ftp.ChangeWorkDir("/data_alarmlist");
        String workDir = "/data_alarmlist/"+ftp_mc_alarmlist_directory;
        ftp.ChangeWorkDir(workDir.c_str());
        String filename_alarmlist = ftp_mc_alarmlist_filename+".txt";
        ftp.NewFile(filename_alarmlist.c_str());
        ftp.Write(fileContent.c_str());
        ftp.CloseFile();
      }
  
}

void SetupFTP(String ip, String user, String pass){

  
      char ftp_server_array[ip.length()];
    ftp_server.toCharArray(ftp_server_array, ip.length() + 1);

    char ftp_user_array[user.length()];
    ftp_user.toCharArray(ftp_user_array, user.length() + 1);

    char ftp_pass_array[pass.length()];
    ftp_pass.toCharArray(ftp_pass_array, pass.length() + 1);
    
    ESP32_FTPClient ftp_temp(ftp_server_array, ftp_user_array, ftp_pass_array, 5000, 2);
    ftp = ftp_temp;
}

void Write_MC_Status(){
  writeFile(SPIFFS, "/p&cmcstatus.txt", prev_mc_status_state.c_str());
}

void Write_Alarm_List(){
  writeFile(SPIFFS, "/p&calarmlist.txt", prev_alarm_list_state.c_str());
}

void Read_MC_Status(){
  String read_mc_status = readFile(SPIFFS, "/p&cmcstatus.txt");
  prev_mc_status_state = getValue(read_mc_status, ',', 0);

}

void Read_Alarm_List(){
  String read_alarm_list = readFile(SPIFFS, "/p&calarmlist.txt");
  prev_alarm_list_state = getValue(read_alarm_list, ',', 0);

}


void WriteTopic(){
    String concateTest = "";
      for(int i = 0;i < 63; i++){
        concateTest = concateTest + topics[i] + ",";
        
      }
      //Serial.print(concateTest);
      writeFile(SPIFFS, "/topicsPath.txt", concateTest.c_str());

      

      //Serial.print(concate);
      
}

void ReadTopic(){
  String readTopics = readFile(SPIFFS, "/topicsPath.txt");
  //Serial.print(readTopics);
    for(int i = 0; i < 63; i++){
    //Serial.println();
    String readVal = getValue(readTopics, ',', i);
    if(readVal == "," || readVal == ""){
         topics[i] = "";
    }else{
      topics[i] = readVal;
    }
    
   }

   for(int i = 0; i < 63; i++){
    topics[i].replace(",","");
    //Serial.println(topics[i]);
    
   }
   
   
}

void ClearAlarmlistData(){
  for(int i = 0;i < 30;i++){
    saved_alarm_list_data[i] = "";
  }
}
