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

#define SPI_PARAM_LOCK() \
    do {                 \
    } while (xSemaphoreTake(paramLock, portMAX_DELAY) != pdPASS)
#define SPI_PARAM_UNLOCK() xSemaphoreGive(paramLock)

// for ESP32 this is the so-called HSPI
// for ESP32-S2/S3/C3 this nomenclature does not really exist anymore,
// it is simply the first externally usable hardware SPI master controller
#define SPI_CMT SPI2_HOST

class esp32_3wSpi {
    public:
        esp32_3wSpi() {
            mInitialized = false;
        }

        void init(uint8_t pinSdio = DEF_CMT_SDIO, uint8_t pinSclk = DEF_CMT_SCLK, uint8_t pinCsb = DEF_CMT_CSB, uint8_t pinFcsb = DEF_CMT_FCSB) {
            paramLock = xSemaphoreCreateMutex();
            spi_bus_config_t buscfg = {
                .mosi_io_num = pinSdio,
                .miso_io_num = -1, // single wire MOSI/MISO
                .sclk_io_num = pinSclk,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = 32,
            };
            spi_device_interface_config_t devcfg = {
                .command_bits = 1,
                .address_bits = 7,
                .dummy_bits = 0,
                .mode = 0,
                .cs_ena_pretrans = 1,
                .cs_ena_posttrans = 1,
                .clock_speed_hz = SPI_CLK,
                .spics_io_num = pinCsb,
                .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
                .queue_size = 1,
                .pre_cb = NULL,
                .post_cb = NULL,
            };

            ESP_ERROR_CHECK(spi_bus_initialize(SPI_CMT, &buscfg, SPI_DMA_DISABLED));
            ESP_ERROR_CHECK(spi_bus_add_device(SPI_CMT, &devcfg, &spi_reg));

            // FiFo
            spi_device_interface_config_t devcfg2 = {
                .command_bits = 0,
                .address_bits = 0,
                .dummy_bits = 0,
                .mode = 0,
                .cs_ena_pretrans = 2,
                .cs_ena_posttrans = (uint8_t)(1 / (SPI_CLK * 10e6 * 2) + 2), // >2 us
                .clock_speed_hz = SPI_CLK,
                .spics_io_num = pinFcsb,
                .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
                .queue_size = 1,
                .pre_cb = NULL,
                .post_cb = NULL,
            };
            ESP_ERROR_CHECK(spi_bus_add_device(SPI_CMT, &devcfg2, &spi_fifo));

            esp_rom_gpio_connect_out_signal(pinSdio, spi_periph_signal[SPI_CMT].spid_out, true, false);
            delay(100);

            //pinMode(pinGpio3, INPUT);
            mInitialized = true;
        }

        void writeReg(uint8_t addr, uint8_t reg) {
            if(!mInitialized)
                return;

            uint8_t tx_data;
            tx_data = ~reg;
            spi_transaction_t t = {
                .cmd = 1,
                .addr = (uint64_t)(~addr),
                .length = 8,
                .tx_buffer = &tx_data,
                .rx_buffer = NULL
            };
            SPI_PARAM_LOCK();
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi_reg, &t));
            SPI_PARAM_UNLOCK();
            delayMicroseconds(100);
        }

        uint8_t readReg(uint8_t addr) {
            if(!mInitialized)
                return 0;

            uint8_t rx_data;
            spi_transaction_t t = {
                .cmd = 0,
                .addr = (uint64_t)(~addr),
                .length = 8,
                .rxlength = 8,
                .tx_buffer = NULL,
                .rx_buffer = &rx_data
            };

            SPI_PARAM_LOCK();
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi_reg, &t));
            SPI_PARAM_UNLOCK();
            delayMicroseconds(100);
            return rx_data;
        }

        void writeFifo(uint8_t buf[], uint8_t len) {
            if(!mInitialized)
                return;
            uint8_t tx_data;

            spi_transaction_t t = {
                .length    = 8,
                .tx_buffer = &tx_data, // reference to write data
                .rx_buffer = NULL
            };

            SPI_PARAM_LOCK();
            for(uint8_t i = 0; i < len; i++) {
                tx_data = ~buf[i]; // negate buffer contents
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi_fifo, &t));
                delayMicroseconds(4); // > 4 us
            }
            SPI_PARAM_UNLOCK();
        }

        void readFifo(uint8_t buf[], uint8_t *len, uint8_t maxlen) {
            if(!mInitialized)
                return;
            uint8_t rx_data;

            spi_transaction_t t = {
                .length = 8,
                .rxlength = 8,
                .tx_buffer = NULL,
                .rx_buffer = &rx_data
            };

            SPI_PARAM_LOCK();
            for(uint8_t i = 0; i < maxlen; i++) {
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi_fifo, &t));
                delayMicroseconds(4); // > 4 us
                if(0 == i)
                    *len = rx_data;
                else
                    buf[i-1] = rx_data;
            }
            SPI_PARAM_UNLOCK();
        }

    private:
        spi_device_handle_t spi_reg, spi_fifo;
        bool mInitialized;
        SemaphoreHandle_t paramLock = NULL;
};
#else
    template<uint8_t CSB_PIN=5, uint8_t FCSB_PIN=4>
    class esp32_3wSpi {
        public:
            esp32_3wSpi() {}
            void setup() {}
            void loop() {}
    };
#endif

#endif /*__ESP32_3WSPI_H__*/
