//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __EPD_HAL_H__
#define __EPD_HAL_H__

#pragma once
#include "../../utils/spiPatcher.h"
#include <esp_rom_gpio.h>
#include <GxEPD2_BW.h>

#define EPD_DEFAULT_SPI_SPEED   4000000 // 4 MHz

class epdHal: public GxEPD2_HalInterface, public SpiPatcherHandle {
    public:
        epdHal() {}

        void patch() override {
            esp_rom_gpio_connect_out_signal(mPinMosi, spi_periph_signal[mHostDevice].spid_out, false, false);
            esp_rom_gpio_connect_in_signal(mPinBusy, spi_periph_signal[mHostDevice].spiq_in, false);
            esp_rom_gpio_connect_out_signal(mPinClk, spi_periph_signal[mHostDevice].spiclk_out, false, false);
        }

        void unpatch() override {
            esp_rom_gpio_connect_out_signal(mPinMosi, SIG_GPIO_OUT_IDX, false, false);
            esp_rom_gpio_connect_in_signal(mPinBusy, GPIO_MATRIX_CONST_ZERO_INPUT, false);
            esp_rom_gpio_connect_out_signal(mPinClk, SIG_GPIO_OUT_IDX, false, false);
        }

        void init(int8_t mosi, int8_t dc, int8_t sclk, int8_t cs, int8_t rst, int8_t busy, int32_t speed = EPD_DEFAULT_SPI_SPEED) {
            mPinMosi = static_cast<gpio_num_t>(mosi);
            mPinDc = static_cast<gpio_num_t>(dc);
            mPinClk = static_cast<gpio_num_t>(sclk);
            mPinCs = static_cast<gpio_num_t>(cs);
            mPinRst = static_cast<gpio_num_t>(rst);
            mPinBusy = static_cast<gpio_num_t>(busy);
            mSpiSpeed = speed;

            mHostDevice = (14 == sclk) ? SPI2_HOST : SPI3_HOST;
            mSpiPatcher = SpiPatcher::getInstance(mHostDevice);

            gpio_reset_pin(mPinMosi);
            gpio_set_direction(mPinMosi, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinMosi, 1);

            gpio_reset_pin(mPinClk);
            gpio_set_direction(mPinClk, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinClk, 0);

            gpio_reset_pin(mPinCs);
            request_spi();
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
            mSpiPatcher->addDevice(mHostDevice, &devcfg, &spi);
            release_spi();

            if(GPIO_NUM_NC != mPinRst) {
                gpio_reset_pin(mPinRst);
                gpio_set_direction(mPinRst, GPIO_MODE_OUTPUT);
                gpio_set_level(mPinRst, HIGH);
            }

            gpio_reset_pin(mPinDc);
            gpio_set_direction(mPinDc, GPIO_MODE_OUTPUT);
            gpio_set_level(mPinDc, HIGH);

            //gpio_reset_pin(mPinBusy);
            //gpio_set_direction(mPinBusy, GPIO_MODE_INPUT);
        }

        void rstMode(uint8_t mode) override {
            if(GPIO_NUM_NC != mPinRst)
                gpio_set_direction(mPinRst, static_cast<gpio_mode_t>(mode));
        }

        void rst(bool level) override {
            if(GPIO_NUM_NC != mPinRst)
                gpio_set_level(mPinRst, level);
        }

        int getBusy(void) override {
            return gpio_get_level(mPinBusy);
        }

        bool isRst(void) override {
            return (GPIO_NUM_NC != mPinRst);
        }

        void write(uint8_t buf) override {
            uint8_t data[1];
            data[0] = buf;
            request_spi();

            size_t spiLen = static_cast<size_t>(1u) << 3;
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
        }

        void write(const uint8_t *buf, uint16_t n) override {
            uint8_t data[n];
            std::copy(&buf[0], &buf[n], &data[0]);

            request_spi();

            size_t spiLen = static_cast<size_t>(n) << 3;
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
        }

        void write(const uint8_t *buf, uint16_t n, int16_t fill_with_zeroes) override {
            uint8_t data[n + fill_with_zeroes];
            memset(data, 0, (n + fill_with_zeroes));
            for (uint16_t i = 0; i < n; i++) {
                data[i] = pgm_read_byte(&*buf++);
            }

            request_spi();
            spi_transaction_t t = {
                .flags = SPI_TRANS_CS_KEEP_ACTIVE,
                .cmd = 0,
                .addr = 0,
                .length = 1u,
                .rxlength = 1u,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };

            size_t offs = 0;
            spi_device_acquire_bus(spi, portMAX_DELAY);
            while(offs < (n + fill_with_zeroes)) {
                t.length = (64u << 3);
                t.rxlength = t.length;
                t.tx_buffer = &data[offs];
                offs += 64;
                ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
            }
            spi_device_release_bus(spi);

            release_spi();
        }

        void writeCmd(const uint8_t val) override {
            uint8_t data[1];
            data[0] = val;

            request_spi();
            gpio_set_level(mPinDc, LOW);

            size_t spiLen = static_cast<size_t>(1u) << 3;
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
            gpio_set_level(mPinDc, HIGH);

            release_spi();
        }

        void writeCmd(const uint8_t *buf, uint8_t n, bool isPGM) override {
            uint8_t data[n-1];
            data[0] = (isPGM) ? pgm_read_byte(&*buf++) : buf[0];

            request_spi();
            gpio_set_level(mPinDc, LOW);
            spi_device_acquire_bus(spi, portMAX_DELAY);

            size_t spiLen = static_cast<size_t>(1u) << 3;
            spi_transaction_t t = {
                .flags = SPI_TRANS_CS_KEEP_ACTIVE,
                .cmd = 0,
                .addr = 0,
                .length = spiLen,
                .rxlength = spiLen,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
            gpio_set_level(mPinDc, HIGH);

            if(isPGM) {
                for (uint16_t i = 0; i < n; i++) {
                    data[i] = pgm_read_byte(&*buf++);
                }
            } else
                std::copy(&buf[1], &buf[n], &data[0]);

            spiLen = static_cast<size_t>(n-1) << 3;
            spi_transaction_t t1 = {
                .flags = SPI_TRANS_CS_KEEP_ACTIVE,
                .cmd = 0,
                .addr = 0,
                .length = spiLen,
                .rxlength = spiLen,
                .user = NULL,
                .tx_buffer = data,
                .rx_buffer = data
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t1));
            spi_device_release_bus(spi);

            release_spi();
        }

        void startTransfer(void) override {
            request_spi();
        }

        void endTransfer(void) override {
            release_spi();
        }

        void transfer(const uint8_t val) override {
            uint8_t data[1];
            data[0] = val;

            size_t spiLen = static_cast<size_t>(1u) << 3;
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
        }

    private:
        inline void request_spi() {
            mSpiPatcher->request(this);
        }

        inline void release_spi() {
            mSpiPatcher->release();
        }

    private:
        gpio_num_t mPinMosi = GPIO_NUM_NC;
        gpio_num_t mPinDc = GPIO_NUM_NC;
        gpio_num_t mPinClk = GPIO_NUM_NC;
        gpio_num_t mPinCs = GPIO_NUM_NC;
        gpio_num_t mPinRst = GPIO_NUM_NC;
        gpio_num_t mPinBusy = GPIO_NUM_NC;
        int32_t mSpiSpeed = EPD_DEFAULT_SPI_SPEED;

        spi_host_device_t mHostDevice;
        spi_device_handle_t spi;
        SpiPatcher *mSpiPatcher;
};

#endif /*__EPD_HAL_H__*/
