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

int buttonManualState = 0;
int buttonCancelState = 0;
int potiPumpInterval = 30UL * 1000UL;
int minPumpInterval = 5;
int maxPumpInterval = 60;
int currentPotiPumpTime = 500;

unsigned long lastDebounceTimeButtonManual = 0;
unsigned long lastDebounceTimeButtonCancel = 0;
unsigned long debounceDelayMs = 50;

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

  anlage.tick(elapsedTime);

  if (anlage.isPumping())
  {
    digitalWrite(PUMP_RELAIS, LOW);
  }
  else
  {
    digitalWrite(PUMP_RELAIS, HIGH);
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
    }
  }

  int readingPotiPumpTime = analogRead(POTI_PUMP_INTERVAL);
  if (currentPotiPumpTime != readingPotiPumpTime){
    currentPotiPumpTime = readingPotiPumpTime;
    unsigned long potiPumpInterval = scaleToTime(readingPotiPumpTime);
    anlage.setPumpTime(potiPumpInterval);
  }

  /*
    if (poti1.IsRotated())
    {
      // Wie lange pumpen
      anlage.setPumpTime(poti1.Value * 0.5);
    }

    if (poti2.IsRotated())
    {
      // Wie lange zwischen 2x pumpen warten
      anlage.setWateringInterval(poti2.Value * 0.5);
    }
  */
}