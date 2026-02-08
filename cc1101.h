#pragma once
#include <Arduino.h>
#include <SPI.h>

class CC1101 {
public:
  CC1101(SPIClass &spi, uint8_t cs, uint8_t gdo0, uint8_t miso);

  void begin();
  uint8_t read_status(uint8_t addr);
  uint8_t status_partnum();
  uint8_t status_version();
  void wait_miso();
  void carrier_sense();
  void serial_data();
  void strobe(uint8_t cmd);
  void freq_433();
  void set_frequency(float mhz);

private:
  SPIClass *_spi;
  int _cs;
  int _gdo0;
  int _miso;

  void select();
  void deselect();
  void write_reg(uint8_t addr, uint8_t value);
};
