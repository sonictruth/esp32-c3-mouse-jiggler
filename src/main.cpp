#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NonBlockingRtttl.h>
#include <BleMouse.h>
#include <esp_sleep.h>
#include <esp_bit_defs.h>

#define BUZZER_PIN 21
#define BUTTON_PIN 5

int lastButtonState = HIGH;

const char *startUpSound = "Mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p;";
const char *shutdownSound = "nokia611:d=4,o=5,b=500:b,a,c";
const char *connectedSound = "beep:d=8,o=5,b=500:c,d,e,f,g,c6;";
const char *beepSound = "beep:d=8,o=5,b=500:c,d";
const char *disconnectedSound = "beep:d=8,o=5,b=500:c6,b,a,g,f,e,d,c;";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define MAX_LINES 2
#define MAX_CHARS_PER_LINE 10
char screenText[MAX_LINES][MAX_CHARS_PER_LINE + 1]; // +1 for null terminator

BleMouse chiupiMouse("ChiupiMouse", "Chiupix", 90);
bool lastConnectionStatus = false;
bool isConnected = false;

short int shutDownTimerSeconds = 5400;
unsigned short int shutDownTimerPresets[] = {5, 1800, 3600, 10800, 14400};

char mainText[100] = "";
char subText[100] = "";

void updateScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(mainText);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(subText);
  display.display();
}

void scrolltext(const char *text)
{
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
  display.startscrollright(0x00, 0x0F);
  delay(1000);
  display.stopscroll();
}

void setup()
{
  Serial.begin(9600);
  delay(1000);
  Serial.println("Setup start");
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  int wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  pinMode(BUZZER_PIN, OUTPUT);

  scrolltext("Ciupi");
  scrolltext("Mouse");

  rtttl::begin(BUZZER_PIN, startUpSound);
  rtttl::play();
  display.ssd1306_command(SSD1306_DISPLAYON);

  chiupiMouse.begin();
  Serial.println("Setup done");
  strcpy(mainText, "Waiting...");
  strcpy(subText, "Search for the Bluetooth device");
}

void shutDown()
{
  chiupiMouse.end();
  delay(500);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  delay(500);

  // esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
  esp_sleep_enable_gpio_wakeup();
  esp_deep_sleep_enable_gpio_wakeup(1 << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

  esp_deep_sleep_start();
}

void checkIfConnectionChanged()
{

  isConnected = chiupiMouse.isConnected();
  if (lastConnectionStatus != isConnected)
  {
    Serial.printf("Connection changed: %d\n", isConnected);
    if (isConnected)
    {
      strcpy(mainText, "Connected");
      rtttl::begin(BUZZER_PIN, connectedSound);
    }
    else
    {
      strcpy(mainText, "Waiting...");
      rtttl::begin(BUZZER_PIN, disconnectedSound);
      chiupiMouse.end();
      delay(300);
      chiupiMouse.begin();
    }
  }
  lastConnectionStatus = isConnected;
}

unsigned short int shutDownTimerSecondsIndex = 0;
void checkButton()
{
  int currentState = digitalRead(BUTTON_PIN);
  if (lastButtonState == LOW && currentState)
  {

    shutDownTimerSeconds = shutDownTimerPresets[shutDownTimerSecondsIndex];
    Serial.printf("shutDownTimerSeconds: %d\n", shutDownTimerSeconds);

    shutDownTimerSecondsIndex = (shutDownTimerSecondsIndex + 1) % 4;
    rtttl::begin(BUZZER_PIN, beepSound);
  }
  lastButtonState = currentState;
}

void moveMouse()
{
  int randomNumber = random(1, 101);
  
  if (randomNumber <= 20 && isConnected) 
  {
    
    int x = random(-2, 2);
    int y = random(-2, 2);
    Serial.print(".");
    chiupiMouse.move(x, y, 0);
  }
}

void handleShutDownTimer()
{
  if (shutDownTimerSeconds >= 0)
  {
    char text[9];

    int hours = shutDownTimerSeconds / 3600;
    int minutes = (shutDownTimerSeconds % 3600) / 60;
    int seconds = shutDownTimerSeconds % 60;

    if (seconds >= 60)
    {
      minutes += seconds / 60;
      seconds %= 60;
    }

    if (minutes >= 60)
    {
      hours += minutes / 60;
      minutes %= 60;
    }

    sprintf(text, "%02d:%02d:%02d", hours, minutes, seconds);

    snprintf(subText, sizeof(subText), "%s", text);
    updateScreen();
    shutDownTimerSeconds--;
  }
  else
  {
    strcpy(mainText, "Bye Bye!");
    strcpy(subText, ";-(");
    rtttl::begin(BUZZER_PIN, shutdownSound);
    updateScreen();
    while (!rtttl::done())
    {
      rtttl::play();
    }
    shutDown();
  }
}

unsigned long lastSlowLoopRunMs = 0;
void runEverySecond()
{
  if (millis() - lastSlowLoopRunMs >= 1000)
  {
    lastSlowLoopRunMs = millis();
    handleShutDownTimer();
    checkIfConnectionChanged();
    moveMouse();
    updateScreen();
  }
}

void loop()
{
  checkButton();
  runEverySecond();
  if (!rtttl::done())
  {
    rtttl::play();
  }
}
