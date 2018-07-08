#include <Arduino.h>
#include <DHT.h>
#include <LowPower.h>
#include <RCSwitch.h>

/* NODE DATA */
const int NODE_HUM_ID = 1;
const int NODE_TEMP_ID = 2;
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

  uint8_t lowBatery = 0;
  if (readVcc() < 2900) {
    lowBatery = 1;
  }

  /*
    Temperature and humidity are send in separated messages due the limitation
    of 24bit data per transmision
    AAAAAAAB CCCCCCCC CCCCCCCC
    AAAAAAA         : ID
    B               : low batery
    CCCCCCCC        : integer value
    CCCCCCCC        : float value First bit determine negative
  */

  uint32_t data[3];

  // humidity
  data[0] = NODE_HUM_ID << 1 | lowBatery;
  data[1] = trunc(h);
  data[2] = abs(static_cast<int>(h * 100)) % 100;

  uint32_t coded = data[0] << 16 | data[1] << 8 | data[2];
  Serial.println(coded, BIN);
  mySwitch.send(coded, 24);
  delay(300);

  // temperature
  data[0] = NODE_TEMP_ID << 1 | lowBatery;
  if (t < 0) {
    data[1] = (uint8_t)abs(trunc(t)) | 0x80;
  } else {
    data[1] = abs(trunc(t));
  }
  data[2] = abs(static_cast<int>(t * 100)) % 100;

  coded = data[0] << 16 | data[1] << 8 | data[2];
  Serial.println(coded, BIN);
  mySwitch.send(coded, 24);
  // sleep
  deepSleep();
}
