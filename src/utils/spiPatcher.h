//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SPI_PATCHER_H__
#define __SPI_PATCHER_H__
#pragma once

#include "spiPatcherHandle.h"

#include <driver/spi_master.h>
#include <freertos/semphr.h>

class SpiPatcher {
    protected:
        SpiPatcher(spi_host_device_t dev) :
            mHostDevice(dev), mCurHandle(nullptr) {
            // Use binary semaphore instead of mutex for performance reasons
            mutex = xSemaphoreCreateBinaryStatic(&mutex_buffer);
            xSemaphoreGive(mutex);

            spi_bus_config_t buscfg = {
                .mosi_io_num = -1,
                .miso_io_num = -1,
                .sclk_io_num = -1,
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
            ESP_ERROR_CHECK(spi_bus_initialize(mHostDevice, &buscfg, SPI_DMA_DISABLED));
        }

    public:
        SpiPatcher(SpiPatcher &other) = delete;
        void operator=(const SpiPatcher &) = delete;

        static SpiPatcher* getInstance(spi_host_device_t dev) {
            if(nullptr == mInstance)
                mInstance = new SpiPatcher(dev);
            return mInstance;
        }

        ~SpiPatcher() { vSemaphoreDelete(mutex); }

        spi_host_device_t getDevice() {
            return mHostDevice;
        }

        inline void request(SpiPatcherHandle* handle) {
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
            xSemaphoreGive(mutex);
        }

    protected:
        static SpiPatcher *mInstance;

    private:
        const spi_host_device_t mHostDevice;
        SpiPatcherHandle* mCurHandle;
        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutex_buffer;
};

#endif /*__SPI_PATCHER_H__*/
