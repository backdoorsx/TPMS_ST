#include "cc1101.h"

#define CC1101_SPI_FREQ 5000000


// CC1101 configuration registers
#define IOCFG0  0x02        // GDO0 Output Pin Configuration
#define FSCTRL1 0X0B        // Frequency Synthesizer Control
#define FSCTRL0 0X0C        // Frequency Synthesizer Control
#define FREQ2   0x0D        // Frequency control word, high byte
#define FREQ1   0x0E        // Frequency control word, middle byte
#define FREQ0   0x0F        // Frequency control word, low byte
 

// CC1101 strobe commands
#define SRES  0x30          // Reset CC1101 chip
#define SRX   0x34          // Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1

// CC1101 Status Registers
#define PARTNUM   0x30      // Part number for CC1101
#define VERSION   0x31      // Current version number 



#define IOCFG0_CARRIER_SENSE  0x0E  // GDO0 Output Pin Configuration - Carrier Sense output
#define IOCFG0_SERIAL_DATA    0x0D  // GDO0 Serial Data Output. Used for asynchronous serial mode.

CC1101::CC1101(SPIClass& spi, uint8_t cs, uint8_t gdo0, uint8_t miso) {
  _spi  = &spi;
  _cs   = cs;
  _gdo0 = gdo0;
  _miso = miso;
}

void CC1101::select() {
  digitalWrite(_cs, LOW);
}

void CC1101::deselect() {
  digitalWrite(_cs, HIGH);
}

void CC1101::begin() {
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  pinMode(_gdo0, INPUT);

  //_spi->begin();

  _spi->beginTransaction(SPISettings(CC1101_SPI_FREQ, MSBFIRST, SPI_MODE0));

  // --- HARD RESET CC1101 ---
  select();
  delayMicroseconds(10);
  deselect();
  delayMicroseconds(40);
  select();

  while (digitalRead(_miso)); // wait_miso();
  _spi->transfer(SRES);
  while (digitalRead(_miso)); // wait_miso();

  deselect();
  delay(1);

  _spi->endTransaction();

  delay(10);
} 

uint8_t CC1101::read_status(uint8_t addr) {
  _spi->beginTransaction(SPISettings(CC1101_SPI_FREQ, MSBFIRST, SPI_MODE0));

  select();
  _spi->transfer(addr | 0xC0);   // READ | STATUS
  uint8_t val = _spi->transfer(0x00);
  deselect();

  _spi->endTransaction();
  return val;
}

void CC1101::wait_miso(){
   uint32_t st = micros();
   while(digitalRead(_miso) == HIGH)
   {
      if (micros() - st > 500)
         break;
   }
}

uint8_t CC1101::status_partnum() {
  return read_status(PARTNUM);
}

uint8_t CC1101::status_version() {
  return read_status(VERSION);
}

void CC1101::write_reg(uint8_t addr, uint8_t value) {
  _spi->beginTransaction(SPISettings(CC1101_SPI_FREQ, MSBFIRST, SPI_MODE0));
  select();
  _spi->transfer(addr);      // WRITE
  _spi->transfer(value);
  deselect();
  _spi->endTransaction();
}

void CC1101::carrier_sense(){
  write_reg(IOCFG0, IOCFG0_CARRIER_SENSE);
}

void CC1101::serial_data(){
  write_reg(IOCFG0, IOCFG0_SERIAL_DATA);
}

void CC1101::set_frequency(float mhz) {
  write_reg(FSCTRL1, 0x0F);
  uint32_t f = (uint32_t)((mhz * 1000000.0) / 396.728515625);
  write_reg(FREQ2, (f >> 16) & 0xFF);
  write_reg(FREQ1, (f >> 8) & 0xFF);
  write_reg(FREQ0, f & 0xFF);
}

void CC1101::strobe(uint8_t cmd) {
  _spi->beginTransaction(SPISettings(CC1101_SPI_FREQ, MSBFIRST, SPI_MODE0));
  select();
  _spi->transfer(cmd);
  deselect();
  _spi->endTransaction();
}
 
