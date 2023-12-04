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
    public:
        explicit SpiPatcher(spi_host_device_t host_device) :
            host_device(host_device), initialized(false), cur_handle(nullptr) {
            // Use binary semaphore instead of mutex for performance reasons
            mutex = xSemaphoreCreateBinaryStatic(&mutex_buffer);
            xSemaphoreGive(mutex);
        }

        ~SpiPatcher() { vSemaphoreDelete(mutex); }

        spi_host_device_t init() {
            if (!initialized) {
                initialized = true;

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
                ESP_ERROR_CHECK(spi_bus_initialize(host_device, &buscfg, SPI_DMA_DISABLED));
            }

            return host_device;
        }

        inline void request(SpiPatcherHandle* handle) {
            xSemaphoreTake(mutex, portMAX_DELAY);

            if (cur_handle != handle) {
                if (cur_handle) {
                    cur_handle->unpatch();
                }
                cur_handle = handle;
                if (cur_handle) {
                    cur_handle->patch();
                }
            }
        }

        inline void release() {
            xSemaphoreGive(mutex);
        }

    private:
        const spi_host_device_t host_device;
        bool initialized;

        SpiPatcherHandle* cur_handle;

        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutex_buffer;
};

#endif /*__SPI_PATCHER_H__*/
