//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __SPI_PATCHER_H__
#define __SPI_PATCHER_H__
#pragma once

#if defined(ESP32)

#include "dbg.h"
#include "spiPatcherHandle.h"

#include <driver/spi_master.h>
#include <freertos/semphr.h>

class SpiPatcher {
    protected:
        explicit SpiPatcher(spi_host_device_t dev) :
            mCurHandle(nullptr) {
            // Use binary semaphore instead of mutex for performance reasons
            mutex = xSemaphoreCreateBinaryStatic(&mutex_buffer);
            xSemaphoreGive(mutex);
            mDev = dev;
            mBusState = ESP_FAIL;
        }

    public:
        SpiPatcher(const SpiPatcher &other) = delete;
        void operator=(const SpiPatcher &) = delete;

        esp_err_t initBus(int mosi = -1, int miso = -1, int sclk = -1, spi_common_dma_t dmaType = SPI_DMA_DISABLED) {
            mBusConfig = spi_bus_config_t {
                .mosi_io_num = mosi,
                .miso_io_num = miso,
                .sclk_io_num = sclk,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .data4_io_num = -1,
                .data5_io_num = -1,
                .data6_io_num = -1,
                .data7_io_num = -1,
                .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
                .flags = 0,
                .intr_flags = 0
            };
            ESP_ERROR_CHECK((mBusState = spi_bus_initialize(mDev, &mBusConfig, dmaType)));

            return mBusState;
        }

        static SpiPatcher* getInstance(spi_host_device_t dev, bool initialize = true) {
            if(SPI2_HOST == dev) {
                if(nullptr == InstanceHost2) {
                    InstanceHost2 = new SpiPatcher(dev);
                    if(initialize)
                        InstanceHost2->initBus();
                }
                return InstanceHost2;
            } else { // SPI3_HOST
                if(nullptr == InstanceHost3) {
                    InstanceHost3 = new SpiPatcher(dev);
                    if(initialize)
                        InstanceHost3->initBus();
                }
                return InstanceHost3;
            }
        }

        ~SpiPatcher() { vSemaphoreDelete(mutex); }

        inline void addDevice(spi_host_device_t host_id, const spi_device_interface_config_t *dev_config, spi_device_handle_t *handle) {
            assert(mBusState == ESP_OK);
            if(SPI2_HOST == host_id)
                mHost2Cnt++;
            if(SPI3_HOST == host_id)
                mHost3Cnt++;

            if((mHost2Cnt > 3) || (mHost3Cnt > 3))
                DPRINTLN(DBG_ERROR, F("maximum number of SPI devices reached (3)"));

            ESP_ERROR_CHECK(spi_bus_add_device(host_id, dev_config, handle));
        }

        inline void request(SpiPatcherHandle* handle) {
            assert(mBusState == ESP_OK);
            xSemaphoreTake(mutex, portMAX_DELAY);

            if (mCurHandle != handle) {
                if (mCurHandle) {
                    mCurHandle->unpatch();
                }
                mCurHandle = handle;
                if (mCurHandle) {
                    mCurHandle->patch();
                }
            }
        }

        inline void release() {
            assert(mBusState == ESP_OK);
            xSemaphoreGive(mutex);
        }

    protected:
        static SpiPatcher *InstanceHost2;
        static SpiPatcher *InstanceHost3;

    private:
        SpiPatcherHandle* mCurHandle;
        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutex_buffer;
        uint8_t mHost2Cnt = 0, mHost3Cnt = 0;
        spi_host_device_t mDev = SPI3_HOST;
        esp_err_t mBusState = ESP_FAIL;
        spi_bus_config_t mBusConfig;
};

#endif /*ESP32*/

#endif /*__SPI_PATCHER_H__*/
