//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __NRF_HAL_H__
#define __NRF_HAL_H__

#pragma once

#include "spiPatcher.h"

#include <esp_rom_gpio.h>
#include <RF24_hal.h>

#define NRF_MAX_TRANSFER_SZ 64
#define NRF_DEFAULT_SPI_SPEED 10000000 // 10 MHz

class nrfHal: public RF24_hal, public SpiPatcherHandle {
    public:
        nrfHal() : mSpiPatcher(SPI2_HOST) {}

        void patch() override {
            esp_rom_gpio_connect_out_signal(mPinMosi, spi_periph_signal[host_device].spid_out, false, false);
            esp_rom_gpio_connect_in_signal(mPinMiso, spi_periph_signal[host_device].spiq_in, false);
            esp_rom_gpio_connect_out_signal(mPinClk, spi_periph_signal[host_device].spiclk_out, false, false);
        }

        void unpatch() override {
            esp_rom_gpio_connect_out_signal(mPinMosi, SIG_GPIO_OUT_IDX, false, false);
            esp_rom_gpio_connect_in_signal(mPinMiso, GPIO_MATRIX_CONST_ZERO_INPUT, false);
            esp_rom_gpio_connect_out_signal(mPinClk, SIG_GPIO_OUT_IDX, false, false);
        }

        void init(int8_t mosi, int8_t miso, int8_t sclk, int8_t cs, int8_t en, int32_t speed = 0) {
            DHEX(mosi);
            DBGPRINT(" ");
            DHEX(miso);
            DBGPRINT(" ");
            DHEX(sclk);
            DBGPRINT(" ");
            DHEX(cs);
            DBGPRINT(" ");
            DHEX(en);
            DBGPRINTLN(" ");
            delay(300);
            mPinMosi = static_cast<>(mosi);
            DBGPRINTLN("21");
            delay(300);
            mPinMiso = static_cast<gpio_num_t>(miso);
            DBGPRINTLN("22");
            delay(300);
            mPinClk = static_cast<gpio_num_t>(sclk);
            DBGPRINTLN("23");
            delay(300);
            mPinCs = static_cast<gpio_num_t>(cs);
            DBGPRINTLN("24");
            delay(300);
            mPinEn = static_cast<gpio_num_t>(en);
            DBGPRINTLN("25");
            delay(300);
            mSpiSpeed = speed;

            DBGPRINTLN("3");
            delay(300);
            host_device = mSpiPatcher.init();
            DBGPRINTLN("4");
            delay(300);

            gpio_reset_pin(mPinMosi);
            gpio_set_direction(mPinMosi, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinMosi, 1);

            gpio_reset_pin(mPinMiso);
            gpio_set_direction(mPinMiso, GPIO_MODE_INPUT);

            gpio_reset_pin(mPinClk);
            gpio_set_direction(mPinClk, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinClk, 0);

            gpio_reset_pin(mPinCs);
            spi_device_interface_config_t devcfg = {
                .command_bits = 0,
                .address_bits = 0,
                .dummy_bits = 0,
                .mode = 0,
                .duty_cycle_pos = 0,
                .cs_ena_pretrans = 0,
                .cs_ena_posttrans = 0,
                .clock_speed_hz = mSpiSpeed,
                .input_delay_ns = 0,
                .spics_io_num = mPinCs,
                .flags = 0,
                .queue_size = 1,
                .pre_cb = nullptr,
                .post_cb = nullptr
            };
            ESP_ERROR_CHECK(spi_bus_add_device(host_device, &devcfg, &spi));

            gpio_reset_pin(mPinEn);
            gpio_set_direction(mPinEn, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinEn, 0);
        }


        bool begin() override {
            return true;
        }

        void end() override {}

        void ce(bool level) override {
            gpio_set_level(mPinEn, level);
        }

        uint8_t write(uint8_t cmd, const uint8_t* buf, uint8_t len) override {
            uint8_t data[NRF_MAX_TRANSFER_SZ];
            data[0] = cmd;
            std::copy(&buf[0], &buf[len], &data[1]);

            request_spi();

            size_t spiLen = (static_cast<size_t>(len) + 1u) << 3;
            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = 0,
                .length = spiLen,
                .rxlength = spiLen,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));

            release_spi();

            return data[0]; // status
        }

        uint8_t write(uint8_t cmd, const uint8_t* buf, uint8_t data_len, uint8_t blank_len) override {
            uint8_t data[NRF_MAX_TRANSFER_SZ];
            data[0] = cmd;
            memset(data, 0, NRF_MAX_TRANSFER_SZ);
            std::copy(&buf[0], &buf[data_len], &data[1]);

            request_spi();

            size_t spiLen = (static_cast<size_t>(data_len) + static_cast<size_t>(blank_len) + 1u) << 3;
            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = 0,
                .length = spiLen,
                .rxlength = spiLen,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));

            release_spi();

            return data[0]; // status
        }

        uint8_t read(uint8_t cmd, uint8_t* buf, uint8_t len) override {
            uint8_t data[NRF_MAX_TRANSFER_SZ];
            data[0] = cmd;
            memset(&data[1], 0xff, len);

            request_spi();

            size_t spiLen = (static_cast<size_t>(len) + 1u) << 3;
            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = 0,
                .length = spiLen,
                .rxlength = spiLen,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));

            release_spi();

            std::copy(&data[1], &data[len+1], buf);
            return data[0]; // status
        }

        uint8_t read(uint8_t cmd, uint8_t* buf, uint8_t data_len, uint8_t blank_len) override {
            uint8_t data[NRF_MAX_TRANSFER_SZ];
            data[0] = cmd;
            memset(&data[1], 0xff, (data_len + blank_len));

            request_spi();

            size_t spiLen = (static_cast<size_t>(data_len) + static_cast<size_t>(blank_len) + 1u) << 3;
            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = 0,
                .length = spiLen,
                .rxlength = spiLen,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));

            release_spi();

            std::copy(&data[1], &data[data_len+1], buf);
            return data[0]; // status
        }

    private:
        inline void request_spi() {
            mSpiPatcher.request(this);
        }

        inline void release_spi() {
            mSpiPatcher.release();
        }

    private:
        gpio_num_t mPinMosi = GPIO_NUM_NC;
        gpio_num_t mPinMiso = GPIO_NUM_NC;
        gpio_num_t mPinClk = GPIO_NUM_NC;
        gpio_num_t mPinCs = GPIO_NUM_NC;
        gpio_num_t mPinEn = GPIO_NUM_NC;
        int32_t mSpiSpeed = NRF_DEFAULT_SPI_SPEED;

        spi_host_device_t host_device;
        spi_device_handle_t spi;
        SpiPatcher mSpiPatcher;
};

#endif /*__NRF_HAL_H__*/
