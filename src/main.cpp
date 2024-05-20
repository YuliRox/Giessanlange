#include <Arduino.h>
#include <nano_pins.h>
#include "Giessanlage.h"

Giessanlage anlage; // Default: 24h Interval, 30sek gießen
unsigned long startTime;
unsigned long elapsedTime;

#define POTI_PUMP_INTERVAL A2
#define BUTTON_MANUAL D2
#define BUTTON_CANCEL D3
#define PUMP_RELAIS D4
#define SWITCH_24H D5
#define SWITCH_12H D6


int switch24hState = 0;
int switch12hState = 0;
int buttonManualState = 0;
int buttonCancelState = 0;
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
  pinMode(BUTTON_MANUAL, INPUT);
  pinMode(BUTTON_CANCEL, INPUT);
  pinMode(PUMP_RELAIS, OUTPUT);

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
  elapsedTime = micros() - startTime;
  startTime = micros();
  outputRemainingWait += elapsedTime;


  anlage.tick(elapsedTime);

  if (anlage.isPumping())
  {
    digitalWrite(PUMP_RELAIS, LOW);
    Serial.print("Power to the Pump!");
  }
  else
  {
    digitalWrite(PUMP_RELAIS, HIGH);
    Serial.print("Depower the Pump!");
  }

  int readingButtonManual = digitalRead(BUTTON_MANUAL);
  if ((millis() - lastDebounceTimeButtonManual) > debounceDelayMs)
  {
    if (readingButtonManual != buttonManualState)
    {
      buttonManualState = readingButtonManual;
    }

    if (buttonManualState == HIGH)
    {
      anlage.triggerPump();
      Serial.println("Open th floodgates!");
    }
  }

  int readingButtonCancel = digitalRead(BUTTON_CANCEL);
  if ((millis() - lastDebounceTimeButtonCancel) > debounceDelayMs)
  {
    if (readingButtonCancel != buttonCancelState)
    {
      buttonCancelState = readingButtonCancel;
    }

    if (buttonCancelState == HIGH)
    {
      anlage.stopPump();
      Serial.println("Cancel!");
    }
  }

  int readingIntervalSwitch = digitalRead(SWITCH_12H);
  if((millis() - lastDebounceTimeIntervalSwitch) > debounceDelayMs){
    if (readingIntervalSwitch != switch12hState) {
      switch12hState = readingIntervalSwitch;

      if (switch12hState == HIGH){
        anlage.setWateringInterval(Giessanlage::INTERVAL_12H);
        Serial.println("New Interval: 12h");
      } else {
        anlage.setWateringInterval(Giessanlage::INTERVAL_24H);
        Serial.println("New Interval: 24h");
      }

      anlage.resetWateringTimer();
    }
  }

  int readingPotiPumpTime = analogRead(POTI_PUMP_INTERVAL);
  if (currentPotiPumpTime != readingPotiPumpTime){
    currentPotiPumpTime = readingPotiPumpTime;
    unsigned long potiPumpInterval = scaleToTime(readingPotiPumpTime);
    anlage.setPumpTime(potiPumpInterval);
    Serial.print("New Watering Time: ");
    Serial.println(potiPumpInterval/1000);
  }


  if (outputRemainingWait >= outputRemainingWaitInterval ){

    if (anlage.getState() == Giessanlage::State::Idle){
      unsigned long remainTime = anlage.getRemainingWateringInterval();
      Serial.print("Remaining Time until next waterin occurs: ");
      Serial.print(remainTime/1000/60);
      Serial.println("min");
    }

    outputRemainingWait = 0;
  }

}