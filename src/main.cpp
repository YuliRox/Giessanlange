#include <Arduino.h>
#include <nano_pins.h>
#include "Giessanlage.h"

// Giessanlage anlage(30UL * 1000UL, 5UL * 1000UL); // Default: 24h Interval, 30sek gießen
Giessanlage anlage; // Default: 24h Interval, 30sek gießen
unsigned long startTime = 0UL;
unsigned long elapsedTime = 0UL;

#define BUTTON_CLOSED LOW
#define BUTTON_OPEN HIGH
#define PUMP_ON LOW
#define PUMP_OFF HIGH
#define POTI_PUMP_INTERVAL A2
#define BUTTON_MANUAL D3
#define BUTTON_CANCEL D2
#define PUMP_RELAIS D4
#define SWITCH_12H D5

int switch12hState = BUTTON_OPEN;
int lastbuttonStateInterval = BUTTON_OPEN;
int buttonManualState = BUTTON_OPEN;
int lastbuttonStateManual = BUTTON_OPEN;
int buttonCancelState = BUTTON_OPEN;
int lastbuttonStateCancel = BUTTON_OPEN;
int potiPumpInterval = 30UL * 1000UL;
const long minPumpInterval = 5;
const long maxPumpInterval = 60;
int currentPotiPumpTime = 500;

unsigned long lastDebounceTimeIntervalSwitch = 0;
unsigned long lastDebounceTimeButtonManual = 0;
unsigned long lastDebounceTimeButtonCancel = 0;
const unsigned long debounceDelayMs = 100UL;

const unsigned long outputRemainingWaitInterval = 60UL * 1000UL;
unsigned long outputRemainingWait = 0;

void setup()
{
  delay(200);

  pinMode(BUTTON_MANUAL, INPUT_PULLUP);
  pinMode(BUTTON_CANCEL, INPUT_PULLUP);
  pinMode(SWITCH_12H, INPUT_PULLUP);
  pinMode(PUMP_RELAIS, OUTPUT);
  digitalWrite(PUMP_RELAIS, PUMP_OFF);

  delay(800);

  Serial.begin(9600);

  // put your setup code here, to run once:
  startTime = millis();
  elapsedTime = startTime;

  delay(1000);

  Serial.println("Hello World Giessanlange!");
  Serial.print("Anlage.State: ");
  Serial.println(anlage.getState());

  Serial.print("RemainingPumpTime: ");
  Serial.print(anlage.getRemainingPumpTime());
  Serial.println("ms");

  Serial.print("RemainingWateringIntervalTime: ");
  Serial.print(anlage.getRemainingWateringInterval());
  Serial.println("ms");

  Serial.print("PumpTime: ");
  Serial.print(anlage.getPumpTime());
  Serial.println("ms");

  Serial.print("WateringInterval: ");
  Serial.print(anlage.getWateringInterval());
  Serial.println("ms");
}

unsigned long scaleToTime(long reading)
{
  long fromPoti = map(reading, 0L, 1023L, minPumpInterval, maxPumpInterval);
  return (unsigned long)fromPoti * 1000UL;
}

void loop()
{
  unsigned long currentTime = millis();
  elapsedTime = currentTime - startTime;

  if (elapsedTime < 100)
  {
    delay(50);
    return;
  }

  startTime = currentTime;
  outputRemainingWait += elapsedTime;

  /*Serial.print("Delta: ");
  Serial.println(elapsedTime);*/

  anlage.tick(elapsedTime);

  /*Serial.print("Anlage.State: ");
  Serial.println(anlage.getState());*/

  if (anlage.isPumping())
  {
    digitalWrite(PUMP_RELAIS, PUMP_ON);
    Serial.println("Power to the Pump!");
  }
  else
  {
    digitalWrite(PUMP_RELAIS, PUMP_OFF);
    //    Serial.println("Depower the Pump!");
  }

  int readingButtonManual = digitalRead(BUTTON_MANUAL);
  // reset the debouncing timer
  if (readingButtonManual != lastbuttonStateManual)
  {
    lastDebounceTimeButtonManual = currentTime;
  }

  if ((currentTime - lastDebounceTimeButtonManual) > debounceDelayMs)
  {
    if (readingButtonManual != buttonManualState)
    {
      buttonManualState = readingButtonManual;
      Serial.print("Toggle green Button: ");
      Serial.println(buttonManualState);

      if (buttonManualState == BUTTON_CLOSED)
      {
        anlage.triggerPump();
        Serial.println("Open the floodgates!");
      }
    }
  }

  lastbuttonStateManual = readingButtonManual;

  int readingButtonCancel = digitalRead(BUTTON_CANCEL);
  if (readingButtonCancel != lastbuttonStateCancel)
  {
    lastDebounceTimeButtonCancel = currentTime;
  }
  if ((currentTime - lastDebounceTimeButtonCancel) > debounceDelayMs)
  {
    if (readingButtonCancel != buttonCancelState)
    {
      buttonCancelState = readingButtonCancel;
      Serial.print("Toggle red Button: ");
      Serial.println(buttonCancelState);

      if (buttonCancelState == BUTTON_CLOSED)
      {
        anlage.stopPump();
        Serial.println("Cancel!");
      }
    }
  }

  lastbuttonStateCancel = readingButtonCancel;

  int readingIntervalSwitch = digitalRead(SWITCH_12H);
  if (readingIntervalSwitch != lastbuttonStateInterval)
  {
    lastDebounceTimeIntervalSwitch = currentTime;
  }
  if ((currentTime - lastDebounceTimeIntervalSwitch) > debounceDelayMs)
  {
    if (readingIntervalSwitch != switch12hState)
    {
      switch12hState = readingIntervalSwitch;

      if (switch12hState == HIGH)
      {
        anlage.setWateringInterval(Giessanlage::INTERVAL_12H);
        Serial.println("New Interval: 12h");
      }
      else
      {
        anlage.setWateringInterval(Giessanlage::INTERVAL_24H);
        Serial.println("New Interval: 24h");
      }

      anlage.resetWateringTimer();
    }
  }

  lastbuttonStateInterval = readingIntervalSwitch;

  int readingPotiPumpTime = analogRead(POTI_PUMP_INTERVAL);
  if (currentPotiPumpTime != readingPotiPumpTime)
  {
    currentPotiPumpTime = readingPotiPumpTime;
    unsigned long potiPumpInterval = scaleToTime(readingPotiPumpTime);
    if (anlage.getPumpTime() != potiPumpInterval)
    {
      anlage.setPumpTime(potiPumpInterval);
      Serial.print("New Pump Time: ");
      Serial.print(potiPumpInterval);
      Serial.println("ms");
    }
  }

  if (outputRemainingWait >= outputRemainingWaitInterval)
  {

    if (anlage.getState() == Giessanlage::State::Idle)
    {
      unsigned long remainTime = anlage.getRemainingWateringInterval();
      Serial.print("Remaining Time until next watering occurs: ");
      Serial.print(remainTime / 1000UL / 60UL);
      Serial.println("min");
    }

    outputRemainingWait = 0;
  }
}