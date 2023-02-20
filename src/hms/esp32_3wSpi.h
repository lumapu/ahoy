//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __ESP32_3WSPI_H__
#define __ESP32_3WSPI_H__

#include "Arduino.h"
#if defined(ESP32)
#include "driver/spi_master.h"
#include "esp_rom_gpio.h" // for esp_rom_gpio_connect_out_signal

#define SPI_CLK     1 * 1000 * 1000 // 1MHz

template<uint8_t CSB_PIN=5, uint8_t FCSB_PIN=4, uint8_t GPIO3_PIN=15>
class esp32_3wSpi {
    public:
        esp32_3wSpi() {}
        void setup() {
            spi_bus_config_t buscfg = {
                .mosi_io_num = MOSI_PIN,
                .miso_io_num = MISO_PIN,
                .sclk_io_num = CLK_PIN,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = 32,
            };
            spi_device_interface_config_t devcfg = {
                .command_bits = 0,
                .address_bits = 0,
                .dummy_bits = 0,
                .mode = 0,                 // SPI mode 0
                .clock_speed_hz = SPI_CLK, // 1 MHz
                .spics_io_num = CSB_PIN,
                .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
                .queue_size = 1,
                .pre_cb = NULL,
                .post_cb = NULL,
            };

            ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, 0));
            ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_reg));

            // FiFo
            spi_device_interface_config_t devcfg2 = {
                .command_bits = 0,
                .address_bits = 0,
                .dummy_bits = 0,
                .mode = 0, // SPI mode 0
                .cs_ena_pretrans = 2,
                .cs_ena_posttrans = (uint8_t)(1 / (SPI_CLK * 10e6 * 2) + 2), // >2 us
                .clock_speed_hz = SPI_CLK,                                   // 1 MHz
                .spics_io_num = FCSB_PIN,
                .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
                .queue_size = 1,
                .pre_cb = NULL,
                .post_cb = NULL,
            };
            ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg2, &spi_fifo));

            esp_rom_gpio_connect_out_signal(MOSI_PIN, spi_periph_signal[SPI2_HOST].spid_out, true, false);
            delay(100);

            pinMode(GPIO3_PIN, INPUT);
        }

        void writeReg(uint8_t addr, uint8_t reg) {
            uint8_t tx_data[2];
            tx_data[0] = ~addr;
            tx_data[1] = ~reg;
            spi_transaction_t t = {
                .length = 2 * 8,
                .tx_buffer = &tx_data,
                .rx_buffer = NULL
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi_reg, &t));
            delayMicroseconds(100);
        }

        uint8_t readReg(uint8_t addr) {
            uint8_t tx_data, rx_data;
            tx_data = ~(addr | 0x80); // negation and MSB high (read command)
            spi_transaction_t t = {
                .length = 8,
                .rxlength = 8,
                .tx_buffer = &tx_data,
                .rx_buffer = &rx_data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi_reg, &t));
            delayMicroseconds(100);
            return rx_data;
        }

        void writeFifo(uint8_t buf[], uint8_t len) {
            uint8_t tx_data;

            spi_transaction_t t = {
                .flags     = SPI_TRANS_MODE_OCT,
                .length    = 8,
                .tx_buffer = &tx_data, // reference to write data
                .rx_buffer = NULL
            };

            for(uint8_t i = 0; i < len; i++) {
                tx_data = ~buf[i]; // negate buffer contents
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi_fifo, &t));
                delayMicroseconds(4); // > 4 us
            }
        }

        void readFifo(uint8_t buf[], uint8_t len) {
            uint8_t rx_data;

            spi_transaction_t t = {
                .length = 8,
                .rxlength = 8,
                .tx_buffer = NULL,
                .rx_buffer = &rx_data
            };

            for(uint8_t i = 0; i < len; i++) {
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi_fifo, &t));
                delayMicroseconds(4); // > 4 us
                buf[i] = rx_data;
            }
        }

    private:
        spi_device_handle_t spi_reg, spi_fifo;
};
#endif

#endif /*__ESP32_3WSPI_H__*/
