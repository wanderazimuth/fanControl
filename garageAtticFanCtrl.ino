/*
 * Rui Santos 
 * Complete Project Details https://randomnerdtutorials.com
 */

// Include the libraries we need
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// signum function
#define sgn(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

// Data wire is connected to GPIO15
#define ONE_WIRE_BUS 4
// Setup a oneWire instance to communicate with a OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// WiFi Credentials
const char* ssid = "palantir";
const char* password = "royandshannamerrellfamily";

// Web Server
AsyncWebServer server(80);

DeviceAddress sensorOutside = { 0x28, 0xC9, 0x1C, 0xB8, 0x7, 0x0, 0x0, 0xBD };
DeviceAddress sensorAttic = { 0x28, 0xAD, 0x51, 0x57, 0x6, 0x0, 0x0, 0xDF };

// Relay GPIO
const int gpioFan = 26;

// Temperatures read from DS18B20 sensors
String sTempOut = "0.0";
String sTempAttic = "0.0";

String tempF2S(float fTemp) {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  //sensors.requestTemperatures(); 
  //float tempF = sensors.getTempF(addr);

  if(fTemp == -196.60){
    Serial.println("Failed to read from DS18B20 sensor");
    return "--";
  } else {
    Serial.print("Temperature Fahrenheit: ");
    Serial.println(fTemp); 
  }
  return String(fTemp);
}

String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPOUT"){
    //return readDsTempF(sensorOutside);
    return sTempOut;
  }
  else if(var == "TEMPATTIC"){
    //return readDsTempF(sensorAttic);
    return sTempAttic;
  }
  return String();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>ESP32 Garage Attic Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Outside</span> 
    <span id="tempOut">%TEMPERATUREOUT%</span>
    <sup class="units">&deg;F</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Garage Attic</span>
    <span id="tempAttic">%TEMPERATUREATTIC%</span>
    <sup class="units">&deg;F</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("tempOut").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/tempout", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("tempAttic").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/tempattic", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";




void setup(void){
  Serial.begin(115200);

  // Initialize GPIO for fan start/stop
  pinMode(gpioFan, OUTPUT);
  
  // Start DS18B20 temp sensors
  sensors.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // Print ES32 Local IP Address
  Serial.println(WiFi.localIP());

  // Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/tempout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", sTempOut.c_str());
  });
  server.on("/tempattic", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", sTempAttic.c_str());
  });
  server.begin();
}


int fanPinState = LOW;
int fanPinStateLast = fanPinState;
const int fanThreshConst = 5;
int fanThresh = fanThreshConst;

void loop(void){
  sensors.requestTemperatures(); 
  float tempOut = sensors.getTempF(sensorOutside);
  float tempAttic = sensors.getTempF(sensorAttic);
  // Save temps as string for web server
  sTempOut = tempF2S(tempOut);
  sTempAttic = tempF2S(tempAttic);

  float errorAttic = 72.0 - tempAttic;
  float tempError = (tempOut - tempAttic) * sgn(errorAttic);
  Serial.println("tempError: "+String(tempError));

  // add hysteresis
  if (fanPinState == LOW) {
    fanThresh = fanThreshConst + 1;
  } else {
    fanThresh = fanThreshConst;
  }
  Serial.println("fanThresh: "+String(fanThresh));

  // Check normal temp switch condition
  if (tempError > fanThresh) {
    fanPinState = HIGH;
  } else {
    fanPinState = LOW;
  }
  // Check attic fire condition
  if (tempAttic > 180) {
    fanPinState = LOW;
  }
  if (fanPinState != fanPinStateLast) {
    Serial.println("Setting fan: "+String(fanPinState));
    digitalWrite(gpioFan, fanPinState);
    fanPinStateLast = fanPinState;
  }
  
  
  delay(2000);
}
