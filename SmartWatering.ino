
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <DHT.h>
#define DHTPIN 2      // Chân dữ liệu của DHT11 kết nối với GPIO2 của ESP8266
#define DHTTYPE DHT11 // Loại DHT được sử dụng
DHT dht(DHTPIN, DHTTYPE);

const int MoisturePin = A0;
const int WaterPumpPin = D2;
const int trigPin = D6;
const int echoPin = D7;
const int buzzPin = D1;
long duration;
int distance, oldDistance;
float Heigh = 11;
String LevelJson;

HTTPClient http;
WiFiClient client;

//const String ssid = "UiTiOt-E3.1";
//const String password = "UiTiOtAP";

const String ssid = "905";
const String password = "12345678";

//const String ssid = "hello";
//const String password = "12345678";

//Server address
const String ServerIP = "192.168.0.101";
const String Host = "http://" + ServerIP + ":5005/api/VariableValues";
const uint16_t Port = 13000;

//VariableValues ID
const int TemperatureId = 2;
const int HumidityId = 3;
const int MoistureId = 1;
const int WaterLevelId = 1002;
int priority = 0;

//Triggers
float temMin, temMax = 40, humMin = 30, humMax, moiMin = 30, moiMax;

//Sensor value
float HumidityValue, TemperatureValue, MoistureValue;

// String DataToSend;
int Interval = 2000;

unsigned long timeout;

void setup()
{
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  pinMode(WaterPumpPin, OUTPUT);
  pinMode(buzzPin, OUTPUT);
  digitalWrite(WaterPumpPin, HIGH); // vì chân D3 nối với relay mà relay tích cực thấp nên mặc định chân D3 phải tích cực cao để ngắt relay

  Serial.println("Connecting to..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  dht.begin();
  Serial.println("Connected");
}
void loop()
{
 
  if (WiFi.status() == WL_CONNECTED)
  {
    if (millis() - timeout > 5000)
    {
       trigger();
      post(DHTJsonData());
      post(MoistureJsonData());
      LevelJson = WaterLevelJsonData();
      if (distance != 0 && distance != oldDistance)
        post(LevelJson);
            Serial.println("Condition:  TemperatureMax: " + String(temMax) + " MoistureMin: " + String(moiMin));
      timeout = millis();
    }
    socketConnection();
  }
}

void post(String DataToSend)
{
  // DataToSend = DHTJsonData();
  Serial.println(DataToSend);

  http.begin(Host);
  http.addHeader("Content-Type", "application/json");
  int status = http.POST(DataToSend); // Send sensor value to server
  Serial.println(status);             // Print status code, response from server
  http.end();
}

void trigger()
{
  if (priority >= 1)
    return;
  if (MoistureValue < moiMin || TemperatureValue > temMax)
  {
    digitalWrite(WaterPumpPin, LOW); // turn the LED on (HIGH is the voltage level)
    delay(Interval);
    digitalWrite(WaterPumpPin, HIGH);
  }
  else
  {
    digitalWrite(WaterPumpPin, HIGH);
  }
}
void handle(String data)
{
  char AB[50];
  data.toCharArray(AB, 50);
  String command = strtok(AB, ":");
  String value[10];
  int i = 0;
  while (command != "")
  {
    value[i] = command;
    data = data.substring(command.length() + 1);
    data.toCharArray(AB, 50);
    command = strtok(AB, ":");
    i++;
  }

  if (value[0] == "T") //T:1:30:50
  {
    char tem[8];
    if (value[1] == String(TemperatureId))
    {
      value[2].toCharArray(tem, value[2].length() + 1);
      temMin = atof(tem);
      value[3].toCharArray(tem, value[3].length() + 1);
      temMax = atof(tem);
    }
    else if (value[1] == String(HumidityId))
    {
      value[2].toCharArray(tem, value[2].length() + 1);
      humMin = atof(tem);
      value[3].toCharArray(tem, value[3].length() + 1);
      humMax = atof(tem);
    }
    else if (value[1] == String(MoistureId))
    {
      value[2].toCharArray(tem, value[2].length() + 1);
      moiMin = atof(tem);
      value[3].toCharArray(tem, value[3].length() + 1);
      moiMax = atof(tem);
    }
  }
  else if (value[0] == "S") // S:1:1
  {
    if (value[1] == "D2")
    {
      digitalWrite(WaterPumpPin, LOW);
      delay(2000);
      digitalWrite(WaterPumpPin, HIGH);
    }
  }
  else if (value[0] == "C") // S:1:1
  {
    if (value[1] == "D2")
    {
      if (value[2] == "1")
      {
        priority = 2;
        digitalWrite(WaterPumpPin, LOW);
      }
      else
      {
        priority = 0;
        digitalWrite(WaterPumpPin, HIGH);
      }
    }
  }
}
void socketConnection()
{
  // Use WiFiClient class to create TCP connections
  if (!client.connected())
  {
    Serial.println("connection failed");
    client.connect(ServerIP, Port);
    client.print(ESP.getFlashChipId());
    delay(5000);
  }
  String result = "";
  while (client.available())
  {
    char ch = static_cast<char>(client.read());
    result += ch;
  }
  if (result != "")
  {
    Serial.println(result);
    handle(result);
  }
}

void readDHTSensor()
{
  HumidityValue = dht.readHumidity();
  TemperatureValue = dht.readTemperature();
  while (isnan(HumidityValue) || isnan(TemperatureValue))
  {
    HumidityValue = dht.readHumidity();
    TemperatureValue = dht.readTemperature();
  }
}

void readMoistureSensor()
{
  MoistureValue = (100 - ((analogRead(MoisturePin) / 1023.00) * 100)); // Convert to moisture percentage
}

void getDistance()
{
  //// Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculating the distance
  distance = duration * 0.034 / 2;

  // // Prints the distance on the Serial Monitor
  Serial.print("Distance: ");
  Serial.println(distance);
  if (distance == 0 || oldDistance > distance)
  {
    distance = oldDistance;
    Serial.println("Dispose");
    return;
  }
  if (distance >= Heigh -1 )
  {
    priority = 1;
    digitalWrite(WaterPumpPin, HIGH);
    digitalWrite(buzzPin, HIGH);
//    Serial.println(" buzz");
  }
  else
  {
//    Serial.println(" unbuzz");
    if(priority!=2)
      priority = 0;
    digitalWrite(buzzPin, LOW);
  }
}
String DHTJsonData()
{
  readDHTSensor();

  const int capacity = JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;

  JsonObject obj1 = doc.createNestedObject();
  obj1["Value"] = TemperatureValue;
  obj1["VariableId"] = TemperatureId;
  JsonObject obj2 = doc.createNestedObject();
  obj2["Value"] = HumidityValue;
  obj2["VariableId"] = HumidityId;

  String result;
  serializeJson(doc, result);
  return result;
}

String MoistureJsonData()
{
  readMoistureSensor();

  const int capacity = JSON_ARRAY_SIZE(1) + 1 * JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;

  JsonObject obj1 = doc.createNestedObject();
  obj1["Value"] = MoistureValue;
  obj1["VariableId"] = MoistureId;

  String result;
  serializeJson(doc, result);
  return result;
}

String WaterLevelJsonData()
{
  getDistance();
  const int capacity = JSON_ARRAY_SIZE(1) + 1 * JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;

  JsonObject obj1 = doc.createNestedObject();
  int percent = ((Heigh - 1 - distance) / Heigh) * 100;
  if (percent <= 0)
    percent = 0;
  obj1["Value"] = percent;
  obj1["VariableId"] = WaterLevelId;

  String result;
  serializeJson(doc, result);
  return result;
}
