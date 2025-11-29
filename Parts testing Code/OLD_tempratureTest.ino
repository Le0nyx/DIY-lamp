#include <DHT.h>

#define DHTPIN 23
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  Serial.println("DHT11 test");
  dht.begin();
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT11!");
    delay(2000);
    return;
  }

  Serial.print("Temp: ");
  Serial.print(t);
  Serial.print(" °C   Humidity: ");
  Serial.print(h);
  Serial.println(" %");

  delay(2000);
}
