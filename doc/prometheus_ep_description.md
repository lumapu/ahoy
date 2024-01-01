# Prometheus Endpoint
Metrics available for AhoyDTU device, inverters and channels.

Prometheus metrics provided at `/metrics`.

## Labels
| Label name   | Description                           |
|:-------------|:--------------------------------------|
| version      | current installed version of AhoyDTU  |
| image        | currently not used                    |
| devicename   | Device name from setup                |
| name         | Inverter name from setup              |
| serial       | Serial number of inverter             |
| inverter     | Inverter name from setup              |
| channel      | Channel (Module) name from setup. Label only available if max power level of module is set to non-zero. Be sure to have a cannel name set in configuration. |

## Exported Metrics
| Metric name                                  | Type    | Description                                              | Labels       |
|----------------------------------------------|---------|----------------------------------------------------------|--------------|
| `ahoy_solar_info`                            | Gauge   | Information about the AhoyDTU device                     | version, image, devicename |
| `ahoy_solar_uptime`                          | Counter | Seconds since boot of the AhoyDTU device                 | devicename |
| `ahoy_solar_freeheap`                        | Gauge   | free heap memory of the AhoyDTU device                   | devicename |
| `ahoy_solar_wifi_rssi_db`                    | Gauge   | Quality of the Wifi STA connection                       | devicename |
| `ahoy_solar_inverter_info`                   | Gauge   | Information about the configured inverter(s)             | name, serial |
| `ahoy_solar_inverter_enabled`                | Gauge   | Is the inverter enabled?                                 | inverter  |
| `ahoy_solar_inverter_is_available`           | Gauge   | is the inverter available?                               | inverter  |
| `ahoy_solar_inverter_is_producing`           | Gauge   | Is the inverter producing?                               | inverter  |
| `ahoy_solar_inverter_power_limit_read`       | Gauge   | Power Limit read from inverter                           | inverter  |
| `ahoy_solar_inverter_power_limit_ack`        | Gauge   | Power Limit acknowledged by inverter                     | inverter  |
| `ahoy_solar_inverter_max_power`              | Gauge   | Max Power of inverter                                    | inverter  |
| `ahoy_solar_inverter_radio_rx_success`       | Counter | NRF24 statistic of inverter                              | inverter  |
| `ahoy_solar_inverter_radio_rx_fail`          | Counter | NRF24 statistic of inverter                              | inverter  |
| `ahoy_solar_inverter_radio_rx_fail_answer`   | Counter | NRF24 statistic of inverter                              | inverter  |
| `ahoy_solar_inverter_radio_frame_cnt`        | Counter | NRF24 statistic of inverter                              | inverter  |
| `ahoy_solar_inverter_radio_tx_cnt`           | Counter | NRF24 statistic of inverter                              | inverter  |
| `ahoy_solar_inverter_radio_retransmits`      | Counter | NRF24 statistic of inverter                              | inverter  |
| `ahoy_solar_U_AC_volt`                       | Gauge   | AC voltage of inverter [V]                               | inverter  |
| `ahoy_solar_I_AC_ampere`                     | Gauge   | AC current of inverter [A]                               | inverter  |
| `ahoy_solar_P_AC_watt`                       | Gauge   | AC power of inverter [W]                                 | inverter  |
| `ahoy_solar_Q_AC_var`                        | Gauge   | AC reactive power[var]                                   | inverter  |
| `ahoy_solar_F_AC_hertz`                      | Gauge   | AC frequency [Hz]                                        | inverter  |
| `ahoy_solar_PF_AC`                           | Gauge   | AC Power factor                                          | inverter  |
| `ahoy_solar_Temp_celsius`                    | Gauge   | Temperature of inverter                                  | inverter  |
| `ahoy_solar_ALARM_MES_ID`                    | Gauge   | Alarm message index of inverter                          | inverter  |
| `ahoy_solar_LastAlarmCode`                   | Gauge   | Last alarm code from inverter                            | inverter  |
| `ahoy_solar_YieldDay_wattHours`              | Counter | Energy converted to AC per day [Wh]                      | inverter,channel  |
| `ahoy_solar_YieldDay_wattHours_total`        | Counter | Energy converted to AC per day [Wh] for all moduls       | inverter  |
| `ahoy_solar_YieldTotal_kilowattHours`        | Counter | Energy converted to AC since reset [kWh]                 | inverter,channel  |
| `ahoy_solar_YieldTotal_kilowattHours_total`  | Counter | Energy converted to AC since reset [kWh] for all modules | inverter  |
| `ahoy_solar_P_DC_watt`                       | Gauge   | DC power of inverter [W]                                 | inverter  |
| `ahoy_solar_Efficiency_ratio`                | Gauge   | ration AC Power over DC Power [%]                        | inverter  |
| `ahoy_solar_U_DC_volt`                       | Gauge   | DC voltage of channel [V]                                | inverter, channel |
| `ahoy_solar_I_DC_ampere`                     | Gauge   | DC current of channel [A]                                | inverter, channel |
| `ahoy_solar_P_DC_watt`                       | Gauge   | DC power of channel [P]                                  | inverter, channel |
| `ahoy_solar_P_DC_watt_total`                 | Gauge   | DC power of inverter [P]                                 | inverter |
| `ahoy_solar_YieldDay_wattHours`              | Counter | Energy converted to AC per day [Wh]                      | inverter, channel |
| `ahoy_solar_YieldTotal_kilowattHours`        | Counter | Energy converted to AC since reset [kWh]                 | inverter, channel |
| `ahoy_solar_Irradiation_ratio`               | Gauge   | ratio DC Power over set maximum power per channel [%]    | inverter, channel |

