#include <Arduino.h>
#include <nano_pins.h>
#include "Giessanlage.h"

Giessanlage anlage; // Default: 24h Interval, 30sek gießen
unsigned long startTime;
unsigned long elapsedTime;

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
int minPumpInterval = 5;
int maxPumpInterval = 60;
int currentPotiPumpTime = 500;

unsigned long lastDebounceTimeIntervalSwitch = 0;
unsigned long lastDebounceTimeButtonManual = 0;
unsigned long lastDebounceTimeButtonCancel = 0;
unsigned long debounceDelayMs = 50;

unsigned long outputRemainingWaitInterval = 60UL * 1000UL;
unsigned long outputRemainingWait = 0;

void setup()
{
  pinMode(BUTTON_MANUAL, INPUT_PULLUP);
  pinMode(BUTTON_CANCEL, INPUT_PULLUP);
  pinMode(SWITCH_12H, INPUT_PULLUP);
  pinMode(PUMP_RELAIS, OUTPUT);
  digitalWrite(PUMP_RELAIS, PUMP_OFF);

  Serial.begin(9600);

  // put your setup code here, to run once:
  startTime = micros();
}

unsigned long scaleToTime(float reading)
{
  int fromPoti = map(reading, 0, 1023, minPumpInterval, maxPumpInterval);
  return (unsigned long)fromPoti * 1000UL;
}

void loop()
{
  unsigned long currentTime = millis();
  elapsedTime = currentTime - startTime;
  startTime = currentTime;
  outputRemainingWait += elapsedTime;

  anlage.tick(elapsedTime);

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
  if (readingButtonManual != lastbuttonStateManual) { lastDebounceTimeButtonManual = currentTime; }

  if ((currentTime - lastDebounceTimeButtonManual) > debounceDelayMs) {
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
    if(readingButtonCancel != lastbuttonStateCancel) { lastDebounceTimeButtonCancel = currentTime;}
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
    if (readingIntervalSwitch != lastbuttonStateInterval) { lastDebounceTimeIntervalSwitch = currentTime; }
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
        Serial.print("New Watering Time: ");
        Serial.println(potiPumpInterval / 1000);
      }
    }

  if (outputRemainingWait >= outputRemainingWaitInterval)
  {

    if (anlage.getState() == Giessanlage::State::Idle)
    {
      unsigned long remainTime = anlage.getRemainingWateringInterval();
      Serial.print("Remaining Time until next waterin occurs: ");
      Serial.print(remainTime / 1000UL / 60UL);
      Serial.println("min");
    }

    outputRemainingWait = 0;
  }
}