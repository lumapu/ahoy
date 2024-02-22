#ifndef __CONFIG_OVERRIDE_H__
#define __CONFIG_OVERRIDE_H__

// save space in the ESP32 by disabling modules

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
    #ifdef PLUGIN_DISPLAY
        #undef PLUGIN_DISPLAY
    #endif

    #ifdef ENABLE_HISTORY
        #undef ENABLE_HISTORY
    #endif
#endif

#endif /*__CONFIG_OVERRIDE_H__*/
