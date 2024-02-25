//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CMT_HAL_H__
#define __CMT_HAL_H__

#pragma once

#include "../utils/spiPatcher.h"

#include <driver/gpio.h>

#define CMT_DEFAULT_SPI_SPEED 4000000 // 4 MHz

class cmtHal : public SpiPatcherHandle {
    public:
        cmtHal() {
            mSpiPatcher = SpiPatcher::getInstance(DEF_CMT_SPI_HOST);
        }

        void patch() override {
            esp_rom_gpio_connect_out_signal(mPinSdio, spi_periph_signal[mHostDevice].spid_out, false, false);
            esp_rom_gpio_connect_in_signal(mPinSdio, spi_periph_signal[mHostDevice].spid_in, false);
            esp_rom_gpio_connect_out_signal(mPinClk, spi_periph_signal[mHostDevice].spiclk_out, false, false);
        }

        void unpatch() override {
            esp_rom_gpio_connect_out_signal(mPinSdio, SIG_GPIO_OUT_IDX, false, false);
            esp_rom_gpio_connect_in_signal(mPinSdio, GPIO_MATRIX_CONST_ZERO_INPUT, false);
            esp_rom_gpio_connect_out_signal(mPinClk, SIG_GPIO_OUT_IDX, false, false);
        }

        void init(int8_t sdio, int8_t clk, int8_t cs, int8_t fcs, int32_t speed = CMT_DEFAULT_SPI_SPEED) {
            mPinSdio  = static_cast<gpio_num_t>(sdio);
            mPinClk   = static_cast<gpio_num_t>(clk);
            mPinCs    = static_cast<gpio_num_t>(cs);
            mPinFcs   = static_cast<gpio_num_t>(fcs);
            mSpiSpeed = speed;

            mHostDevice = mSpiPatcher->getDevice();

            gpio_reset_pin(mPinSdio);
            gpio_set_direction(mPinSdio, GPIO_MODE_INPUT_OUTPUT);
            gpio_set_level(mPinSdio, 1);

            gpio_reset_pin(mPinClk);
            gpio_set_direction(mPinClk, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinClk, 0);

            gpio_reset_pin(mPinCs);
            spi_device_interface_config_t devcfg_reg = {
                .command_bits = 1,
                .address_bits = 7,
                .dummy_bits = 0,
                .mode = 0,
                .duty_cycle_pos = 0,
                .cs_ena_pretrans = 1,
                .cs_ena_posttrans = 1,
                .clock_speed_hz = mSpiSpeed,
                .input_delay_ns = 0,
                .spics_io_num = mPinCs,
                .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
                .queue_size = 1,
                .pre_cb = nullptr,
                .post_cb = nullptr
            };
            ESP_ERROR_CHECK(spi_bus_add_device(mHostDevice, &devcfg_reg, &spi_reg));

            gpio_reset_pin(mPinFcs);
            spi_device_interface_config_t devcfg_fifo = {
                .command_bits = 0,
                .address_bits = 0,
                .dummy_bits = 0,
                .mode = 0,
                .duty_cycle_pos = 0,
                .cs_ena_pretrans = 2,
                .cs_ena_posttrans = static_cast<uint8_t>(2 * mSpiSpeed / 1000000), // >2 us
                .clock_speed_hz = mSpiSpeed,
                .input_delay_ns = 0,
                .spics_io_num = mPinFcs,
                .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
                .queue_size = 1,
                .pre_cb = nullptr,
                .post_cb = nullptr
            };
            ESP_ERROR_CHECK(spi_bus_add_device(mHostDevice, &devcfg_fifo, &spi_fifo));
        }

        uint8_t readReg(uint8_t addr) {
            uint8_t data = 0;

            request_spi();

            spi_transaction_t t = {
                .flags = 0,
                .cmd = 1,
                .addr = addr,
                .length = 0,
                .rxlength = 8,
                .user = NULL,
                .tx_buffer = NULL,
                .rx_buffer = &data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi_reg, &t));

            release_spi();

            return data;
        }

        void writeReg(uint8_t addr, uint8_t data) {
            request_spi();

            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = addr,
                .length = 8,
                .rxlength = 0,
                .user = NULL,
                .tx_buffer = &data,
                .rx_buffer = NULL
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi_reg, &t));

            release_spi();
        }

        void readFifo(uint8_t buf[], uint8_t *len, uint8_t maxlen) {
            request_spi();

            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = 0,
                .length = 0,
                .rxlength = 8,
                .user = NULL,
                .tx_buffer = NULL,
                .rx_buffer = NULL
            };
            for (uint8_t i = 0; i < maxlen; i++) {
                if(0 == i)
                    t.rx_buffer = len;
                else
                    t.rx_buffer = buf + i - 1;
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi_fifo, &t));
            }

            release_spi();
        }
        void writeFifo(const uint8_t buf[], uint16_t len) {
            request_spi();

            spi_transaction_t t = {
                .flags = 0,
                .cmd = 0,
                .addr = 0,
                .length = 8,
                .rxlength = 0,
                .user = NULL,
                .tx_buffer = NULL,
                .rx_buffer = NULL
            };
            for (uint16_t i = 0; i < len; i++) {
                t.tx_buffer = buf + i;
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi_fifo, &t));
            }

            release_spi();
        }

    private:
        inline void request_spi() {
            mSpiPatcher->request(this);
        }

        inline void release_spi() {
            mSpiPatcher->release();
        }

    private:
        gpio_num_t mPinSdio = GPIO_NUM_NC;
        gpio_num_t mPinClk  = GPIO_NUM_NC;
        gpio_num_t mPinCs   = GPIO_NUM_NC;
        gpio_num_t mPinFcs  = GPIO_NUM_NC;
        int32_t mSpiSpeed   = CMT_DEFAULT_SPI_SPEED;

        spi_host_device_t mHostDevice;
        spi_device_handle_t spi_reg, spi_fifo;
        SpiPatcher *mSpiPatcher;
};

#endif /*__CMT_HAL_H__*/
