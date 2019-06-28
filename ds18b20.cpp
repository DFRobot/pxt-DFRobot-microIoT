/*
 * micro:bit OneWire Library, derived from the mbed DS1820 Library, for the
 * Dallas (Maxim) 1-Wire Digital Thermometer
 * Copyright (c) 2010, Michael Hagberg Michael@RedBoxCode.com
 *
 * This version uses a single instance to talk to multiple one wire devices.
 * During configuration the devices will be listed and the addresses
 * then stored within the system  they are associated with.
 *
 * Then previously stored addresses are used to query devices.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include "pxt.h"
#include "TimedInterruptIn.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include <cstdlib>

using namespace pxt;

namespace DS18B20 {

  #define FAMILY_CODE address.rom[0]
  //#define FAMILY_CODE 0x28
  #define FAMILY_CODE_DS18S20 0x10 //9bit temp
  #define FAMILY_CODE_DS18B20 0x28 //9-12bit temp also known as MAX31820
  #define FAMILY_CODE_DS1822  0x22 //9-12bit temp
  #define FAMILY_CODE_MAX31826 0x3B //12bit temp + 1k EEPROM
  #define FAMILY_CODE_DS2404 0x04 //RTC
  #define FAMILY_CODE_DS2417 0x27 //RTC
  #define FAMILY_CODE_DS2740 0x36 //Current measurement
  #define FAMILY_CODE_DS2502 0x09 //1k EEPROM

  static const int ReadScratchPadCommand = 0xBE;
  static const int ReadPowerSupplyCommand = 0xB4;
  static const int ConvertTempCommand = 0x44;
  static const int MatchROMCommand = 0x55;
  static const int ReadROMCommand = 0x33;
  static const int SearchROMCommand = 0xF0;
  static const int SkipROMCommand = 0xCC;
  static const int WriteScratchPadCommand = 0x4E;
  struct rom_address_t {
      uint8_t rom[8];
  };

  std::vector<rom_address_t> found_addresses;

  class OneWire {
  public:

    enum {
        invalid_conversion = -1000
    };

    OneWire(PinName data_pin, PinName power_pin = NC, bool power_polarity = 0) : _datapin(data_pin),_parasitepin(power_pin) {
      _power_polarity = power_polarity;
      _power_mosfet = power_pin != NC;
    }

    ~OneWire(void) {
      found_addresses.clear();
    }

    void init() {
      int byte_counter;

      for (byte_counter = 0; byte_counter < 9; byte_counter++)
          RAM[byte_counter] = 0x00;

      rom_address_t address;
      _parasite_power = !powerSupplyAvailable(address, true);
    }

    int findAllDevicesOnBus() {
      while (searchRomFindNext()) {
      }
      return (int) found_addresses.size();
    }

    rom_address_t &getAddress(int index) {
      return found_addresses[index];
    }

    int convertTemperature(rom_address_t &address, bool wait, bool all) {
      // Convert temperature into scratchpad RAM for all devices at once
      int delay_time = 1; // Default delay time
      uint8_t resolution;
      if (all)
        skip_ROM();          // Skip ROM command, will convert for ALL devices, wait maximum time
      else {
        match_ROM(address);
        if ((FAMILY_CODE == FAMILY_CODE_DS18B20) || (FAMILY_CODE == FAMILY_CODE_DS1822)) {
          resolution = (uint8_t) (RAM[4] & 0x60);
          if (resolution == 0x00) // 9 bits
            delay_time = 94;
          if (resolution == 0x20) // 10 bits
            delay_time = 188;
          if (resolution == 0x40) // 11 bits. Note 12bits uses the 750ms default
            delay_time = 375;
        }
        if (FAMILY_CODE == FAMILY_CODE_MAX31826) {
          delay_time = 150; // 12bit conversion
        }
      }

      onewire_byte_out(ConvertTempCommand);  // perform temperature conversion
      if (_parasite_power) {
        if (_power_mosfet) {
          _parasitepin.write(_power_polarity);     // Parasite power strong pullup
          wait_ms(delay_time);
          _parasitepin.write(!_power_polarity);
          delay_time = 0;
        } else {
          _datapin.output();
          _datapin.write(1);
          wait_ms(delay_time);
          _datapin.input();
        }
      } else {
          if (wait) {
            wait_ms(delay_time);
            delay_time = 0;
        }
      }
      return delay_time;
    }


    int temperature(rom_address_t &address) {
      //float answer, remaining_count, count_per_degree;
      int reading = 0;
      readScratchPad(address);
/*
      if (RAM_checksum_error()){
        // Indicate we got a CRC error
        answer = invalid_conversion;
      }
      else {
        reading = (RAM[1] << 8) + RAM[0];
        if (reading & 0x8000) { // negative degrees C
          reading = 0 - ((reading ^ 0xffff) + 1); // 2's comp then convert to signed int
        }
        answer = (float)reading;
        switch (FAMILY_CODE) {
          case FAMILY_CODE_MAX31826:
          case FAMILY_CODE_DS18B20:
          case FAMILY_CODE_DS1822:
            answer = answer / 16.0f;
            break;
          case FAMILY_CODE_DS18S20:
            remaining_count = RAM[6];
            count_per_degree = RAM[7];
            answer = (float) (floor(answer / 2.0f) - 0.25f +
                              (count_per_degree - remaining_count) / count_per_degree);
            break;
          default:
            //uBit.serial.printf("Unknown device family");
            break;
        }
        if (convertToFarenheight) {
            answer = answer * 9.0f / 5.0f + 32.0f;
        }
      }
*/
    reading = (RAM[1] << 8) + RAM[0];
      return reading*100/16;
    }

    bool setResolution(rom_address_t &address, unsigned int resolution) {
      bool answer = false;
      switch (FAMILY_CODE) {
        case FAMILY_CODE_DS18B20:
        case FAMILY_CODE_DS18S20:
        case FAMILY_CODE_DS1822:
          resolution = resolution - 9;
          if (resolution < 4) {
            resolution = resolution << 5; // align the bits
            RAM[4] = (uint8_t) ((RAM[4] & 0x60) | resolution); // mask out old data, insert new
            writeScratchPad(address, (RAM[2] << 8) + RAM[3]);
            answer = true;
          }
          break;
        default:
          break;
      }
      return answer;
    }

    void singleDeviceReadROM(rom_address_t &address) {
      if (!onewire_reset()) {
        return;
      } else {
        onewire_byte_out(ReadROMCommand);
        for (int bit_index = 0; bit_index < 64; bit_index++) {
          bool bit = onewire_bit_in();
          bitWrite((uint8_t &) address.rom[bit_index / 8], (bit_index % 8), bit);
        }
      }
    }

    static rom_address_t addressFromHex(const char *hexAddress) {
      rom_address_t address = rom_address_t();
      for (uint8_t i = 0; i < sizeof(address.rom); i++) {
        char buffer[3];
        strncpy(buffer, &hexAddress[i * 2], 2);
        buffer[2] = '\0';
        address.rom[i] = (uint8_t) strtol(buffer, NULL, 16);
      }
      return address;
    }

  private:
    DigitalInOut _datapin;
    DigitalOut _parasitepin;
    bool _parasite_power;
    bool _power_mosfet;
    bool _power_polarity;
    uint8_t RAM[9];

    uint8_t CRC_byte(uint8_t _CRC, uint8_t byte) {
      int j;
      for (j = 0; j < 8; j++) {
        if ((byte & 0x01) ^ (_CRC & 0x01)) {
          // DATA ^ LSB CRC = 1
          _CRC = _CRC >> 1;
          // Set the MSB to 1
          _CRC = (uint8_t) (_CRC | 0x80);
          // Check bit 3
          if (_CRC & 0x04) {
            _CRC = (uint8_t) (_CRC & 0xFB); // Bit 3 is set, so clear it
          } else {
            _CRC = (uint8_t) (_CRC | 0x04); // Bit 3 is clear, so set it
          }
          // Check bit 4
          if (_CRC & 0x08) {
            _CRC = (uint8_t) (_CRC & 0xF7); // Bit 4 is set, so clear it
          } else {
            _CRC = (uint8_t) (_CRC | 0x08); // Bit 4 is clear, so set it
          }
        } else {
          // DATA ^ LSB CRC = 0
          _CRC = _CRC >> 1;
          // clear MSB
          _CRC = (uint8_t) (_CRC & 0x7F);
          // No need to check bits, with DATA ^ LSB CRC = 0, they will remain unchanged
        }
        byte = byte >> 1;
      }
      return _CRC;
    }

    void bitWrite(uint8_t &value, int bit, bool set) {
      if (bit <= 7 && bit >= 0) {
        if (set) {
          value |= (1 << bit);
        } else {
          value &= ~(1 << bit);
        }
      }
    }


    bool onewire_reset() {
    // This will return false if no devices are present on the data bus
      bool presence = false;
      _datapin.output();
      _datapin.write(0);          // bring low for 480 us
      wait_us(480);
      _datapin.write(1);
      wait_us(20);
      _datapin.input();       // let the data line float high
      wait_us(25);
      if (_datapin.read() == 0) // see if any devices are pulling the data line low
        presence = true;
      wait_us(120);
      return presence;
    }

    void match_ROM(rom_address_t &address) {
      int i;
      onewire_reset();
      onewire_byte_out(MatchROMCommand);
      for (i = 0; i < 8; i++) {
        onewire_byte_out(address.rom[i]);
      }
    }

    void skip_ROM() {
      onewire_reset();
      onewire_byte_out(SkipROMCommand);
      //onewire_byte_out(0x20);
    }

    void onewire_bit_out(bool bit_data) {
      _datapin.output();
      __disable_irq();
      _datapin.write(0);
      wait_us(3);                 // DXP modified from 5 to 3 (spec 1-15us)
      if (bit_data) {
        _datapin.write(1); // bring data line high
        __enable_irq();
        wait_us(55);
      } else {
        wait_us(60);            // keep data line low (spec 60-120us)
        _datapin.write(1);
        __enable_irq();
        wait_us(5);            // DXP added 10 to allow bus to float high before next bit_out
      }
    }

    void onewire_byte_out(uint8_t data) {
      int n;
      for (n = 0; n < 8; n++) {
        onewire_bit_out((bool) (data & 0x01));
        data = data >> 1; // now the next bit is in the least sig bit position.
      }
    }

    bool onewire_bit_in() {
      bool answer;
      _datapin.output();
      __disable_irq();
      _datapin.write(0);
      wait_us(3);                 // DXP modified from 5 (spec 1-15us)
      _datapin.input();
      wait_us(3);                // DXP modified from 5 to 10 this broke microbit timing (spec read within 15us)
      answer = (bool) _datapin.read();
      __enable_irq();
      wait_us(45);                // DXP modified from 50 to 45, but Arduino uses 53?
      return answer;
    }

    uint8_t onewire_byte_in() {
      uint8_t answer = 0x00;
      int i;
      for (i = 0; i < 8; i++) {
        answer = answer >> 1; // shift over to make room for the next bit
        if (onewire_bit_in())
          answer = (uint8_t) (answer | 0x80); // if the data port is high, make this bit a 1
      }
      return answer;
    }

    bool ROM_checksum_error(uint8_t *_ROM_address) {
      uint8_t _CRC = 0x00;
      int i;
      for (i = 0; i < 7; i++) // Only going to shift the lower 7 bytes
        _CRC = CRC_byte(_CRC, _ROM_address[i]);
      // After 7 bytes CRC should equal the 8th byte (ROM CRC)
      return (_CRC != _ROM_address[7]); // will return true if there is a CRC checksum mis-match
    }

    bool RAM_checksum_error() {
      uint8_t _CRC = 0x00;
      int i;
      for (i = 0; i < 8; i++) // Only going to shift the lower 8 bytes
        _CRC = CRC_byte(_CRC, RAM[i]);
      // After 8 bytes CRC should equal the 9th byte (RAM CRC)
      return (_CRC != RAM[8]); // will return true if there is a CRC checksum mis-match
    }

    bool searchRomFindNext() {
      bool DS1820_done_flag = false;
      int ds1820_last_descrepancy = 0;
      uint8_t DS1820_search_ROM[8] = {0, 0, 0, 0, 0, 0, 0, 0};

      int descrepancyMarker, ROM_bit_index;
      bool return_value, Bit_A, Bit_B;
      uint8_t byte_counter, bit_mask;

      return_value = false;
      while (!DS1820_done_flag) {
        if (!onewire_reset()) {
          //uBit.serial.printf("Failed to reset one wire bus\n");
          return false;
        } else {
          ROM_bit_index = 1;
          descrepancyMarker = 0;
          onewire_byte_out(SearchROMCommand);
          byte_counter = 0;
          bit_mask = 0x01;
          while (ROM_bit_index <= 64) {
            Bit_A = onewire_bit_in();
            Bit_B = onewire_bit_in();
            if (Bit_A & Bit_B) {
              descrepancyMarker = 0; // data read error, this should never happen
              ROM_bit_index = 0xFF;
              //uBit.serial.printf("Data read error - no devices on bus?\r\n");
            } else {
              if (Bit_A | Bit_B) {
                // Set ROM bit to Bit_A
                if (Bit_A) {
                  DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] | bit_mask; // Set ROM bit to one
                } else {
                  DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] & ~bit_mask; // Set ROM bit to zero
                }
              } else {
                // both bits A and B are low, so there are two or more devices present
                if (ROM_bit_index == ds1820_last_descrepancy) {
                  DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] | bit_mask; // Set ROM bit to one
                } else {
                  if (ROM_bit_index > ds1820_last_descrepancy) {
                      DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] & ~bit_mask; // Set ROM bit to zero
                      descrepancyMarker = ROM_bit_index;
                  } else {
                    if ((DS1820_search_ROM[byte_counter] & bit_mask) == 0x00)
                      descrepancyMarker = ROM_bit_index;
                  }
                }
              }
              onewire_bit_out(DS1820_search_ROM[byte_counter] & bit_mask);
              ROM_bit_index++;
              if (bit_mask & 0x80) {
                byte_counter++;
                bit_mask = 0x01;
              } else {
                bit_mask = bit_mask << 1;
              }
            }
          }
          ds1820_last_descrepancy = descrepancyMarker;
          if (ROM_bit_index != 0xFF) {
            uint8_t i = 0;
            while (1) {
              if (i >= found_addresses.size()) {                             //End of list, or empty list
                if (ROM_checksum_error(DS1820_search_ROM)) {          // Check the CRC
                  //uBit.serial.printf("failed crc\r\n");
                  return false;
                }
                rom_address_t address;
                for (byte_counter = 0; byte_counter < 8; byte_counter++) {
                  address.rom[byte_counter] = DS1820_search_ROM[byte_counter];
                }
                found_addresses.push_back(address);

                return true;
              } else {                    //Otherwise, check if ROM is already known
                bool equal = true;
                uint8_t *ROM_compare = found_addresses[i].rom;

                for (byte_counter = 0; (byte_counter < 8) && equal; byte_counter++) {
                  if (ROM_compare[byte_counter] != DS1820_search_ROM[byte_counter])
                    equal = false;
                }
                if (equal)
                  break;
                else
                  i++;
              }
            }
          }
        }
        if (ds1820_last_descrepancy == 0)
          DS1820_done_flag = true;
      }
      return return_value;
    }

    void readScratchPad(rom_address_t &address) {
      int i;
      match_ROM(address);
      onewire_byte_out(ReadScratchPadCommand);
      for (i = 0; i < 9; i++) {
        RAM[i] = onewire_byte_in();
      }
    }

    void writeScratchPad(rom_address_t &address, int data) {
      RAM[3] = (uint8_t) data;
      RAM[2] = (uint8_t) (data >> 8);
      match_ROM(address);
      onewire_byte_out(WriteScratchPadCommand);
      onewire_byte_out(RAM[2]); // T(H)
      onewire_byte_out(RAM[3]); // T(L)
      if ((FAMILY_CODE == FAMILY_CODE_DS18S20)|| (FAMILY_CODE == FAMILY_CODE_DS18B20) || (FAMILY_CODE == FAMILY_CODE_DS1822)) {
        onewire_byte_out(RAM[4]); // Configuration register
      }
    }

    bool powerSupplyAvailable(rom_address_t &address, bool all) {
      if (all)
        skip_ROM();
      else
        match_ROM(address);
      onewire_byte_out(ConvertTempCommand);
      wait_ms(1);//////////////////
      return onewire_bit_in();
    }
  };

  //MicroBit uBit;
  MicroBitPin pin = uBit.io.P0;
  //%
  int16_t Temperature(int p) {
    
    switch(p){
      case 0: pin = uBit.io.P0; break;
      case 1: pin = uBit.io.P1; break;
      case 2: pin = uBit.io.P2; break;
      case 5: pin = uBit.io.P5; break;
      case 8: pin = uBit.io.P8; break;
      case 11: pin = uBit.io.P11; break;
      case 12: pin = uBit.io.P12; break;
      case 13: pin = uBit.io.P13; break;
      case 14: pin = uBit.io.P14; break;
      case 15: pin = uBit.io.P15; break;
      case 16: pin = uBit.io.P16; break;
      default: pin = uBit.io.P0;
    }
    
    OneWire oneWire(pin.name);
    oneWire.init();
    oneWire.findAllDevicesOnBus();
    rom_address_t address;
    oneWire.singleDeviceReadROM(address);
    oneWire.convertTemperature(address, true, true);
    return oneWire.temperature(address);
    //return 0;
  }
}