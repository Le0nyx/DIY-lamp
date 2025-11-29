#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels
#define OLED_RESET    -1 // Reset pin (not used with I2C)
#define SCREEN_ADDRESS 0x3C // I2C address (common for SSD1306)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Wire.begin(18, 19); // SDA = 18, SCL = 19 (adjust if needed)
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
}

void loop() {
  display.clearDisplay();

  // Draw flashing rectangle
  static bool toggle = false;
  if(toggle) {
    display.fillRect(10, 10, 50, 30, SSD1306_WHITE);
  } else {
    display.drawRect(10, 10, 50, 30, SSD1306_WHITE);
  }
  toggle = !toggle;

  // Optional: draw some text
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 50);
  display.println(F("Flashing Demo"));

  display.display();
  delay(500); // half-second flash
}
