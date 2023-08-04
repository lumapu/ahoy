#ifndef _SML_OBIS_PARSER_H_
#define _SML_OBIS_PARSER_H_

#ifdef AHOY_SML_OBIS_SUPPORT

#include "appInterface.h"


void sml_setup (IApp *app, uint32_t *timestamp);
void sml_cleanup_history ();
File sml_open_hist ();
void sml_close_hist (File file);
int sml_find_hist_power (File file, uint16_t index);
int16_t sml_get_obis_pac ();
int16_t sml_get_obis_pac_average ();
uint16_t sml_parse_stream (uint16 len);
void sml_set_trace_obis (bool trace_flag);
void sml_loop();

#endif

#endif