#include <Arduino.h>
#include <DHT.h>
#include <LowPower.h>
#include <RCSwitch.h>

/* NODE DATA */
const uint8_t NODE_HUM_ID = 1;
const uint8_t NODE_TEMP_ID = 2;
const int PULSE_LENGTH = 320;

/* hardware data */
const int rfPin = 9;
const int DHTPin = 6;
const int SLEEP_TIME = 39; // (300s/8) -> 5 minutes
RCSwitch mySwitch = RCSwitch();
#define DHTTYPE DHT22
DHT dht(DHTPin, DHTTYPE);

long readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC))
    ;

  long result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  Serial.println(result, DEC);
  return result;
}

void deepSleep() {
    for (int i = 0; i < SLEEP_TIME; i++) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON);
    }
}

void setup() {
  Serial.begin(9600);
  mySwitch.enableTransmit(rfPin);

  mySwitch.setPulseLength(PULSE_LENGTH);
  // mySwitch.setProtocol(4);
  dht.begin();
}

void loop() {

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    deepSleep();
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");

  bool lowBatery = 0;
  if (readVcc() < 2900) {
    lowBatery = 1;
  }

  /*
    Temperature and humidity are send in separated messages due the limitation
    of 24bit data per transmision
    AAAAAAAB CCCCCCCC CCCCCCCC
    AAAAAAA         : ID
    B               : low batery
    CCCCCCCC        : signed integer value
    CCCCCCCC        : floating part value
  */

 byte data[3];

  //due casting byte array start reading from length-1, the position of data is inverted
  // humidity
  data[2] = (NODE_HUM_ID << 1 | lowBatery);
  data[1] = (int8_t)trunc(h);
  data[0] = (int8_t)(abs(static_cast<int>(h * 100)) % 100);

  Serial.println(*((unsigned long*)data), BIN);
  mySwitch.send(*((unsigned long*)data), 24);
  delay(300);

  // temperature
  data[2] = (uint8_t)(NODE_TEMP_ID << 1 | lowBatery);
  data[1] = (int8_t)trunc(t);
  data[0] = (int8_t)(abs(static_cast<int>(t * 100)) % 100);

  Serial.println(*((unsigned long*)data), BIN);
  mySwitch.send(*((unsigned long*)data), 24);
  // sleep
  deepSleep();
}
