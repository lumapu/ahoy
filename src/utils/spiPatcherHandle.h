//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SPI_PATCHER_HANDLE_H__
#define __SPI_PATCHER_HANDLE_H__
#pragma once

class SpiPatcherHandle {
    public:
        virtual ~SpiPatcherHandle() {}
        virtual void patch() = 0;
        virtual void unpatch() = 0;
};

#endif /*__SPI_PATCHER_HANDLE_H__*/
