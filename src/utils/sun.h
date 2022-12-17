//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SUN_H__
#define __SUN_H__

namespace ah {
    void calculateSunriseSunset(uint32_t utcTs, uint32_t offset, float lat, float lon, uint32_t *sunrise, uint32_t *sunset) {
        // Source: https://en.wikipedia.org/wiki/Sunrise_equation#Complete_calculation_on_Earth

        // Julian day since 1.1.2000 12:00
        double n_JulianDay = (utcTs + offset) / 86400 - 10957.0;
        // Mean solar time
        double J = n_JulianDay - lon / 360;
        // Solar mean anomaly
        double M = fmod((357.5291 + 0.98560028 * J), 360);
        // Equation of the center
        double C = 1.9148 * SIN(M) + 0.02 * SIN(2 * M) + 0.0003 * SIN(3 * M);
        // Ecliptic longitude
        double lambda = fmod((M + C + 180 + 102.9372), 360);
        // Solar transit
        double Jtransit = 2451545.0 + J + 0.0053 * SIN(M) - 0.0069 * SIN(2 * lambda);
        // Declination of the sun
        double delta = ASIN(SIN(lambda) * SIN(23.44));
        // Hour angle
        double omega = ACOS((SIN(-3.5) - SIN(lat) * SIN(delta)) / (COS(lat) * COS(delta))); //(SIN(-0.83)
        // Calculate sunrise and sunset
        double Jrise = Jtransit - omega / 360;
        double Jset = Jtransit + omega / 360;
        // Julian sunrise/sunset to UTC unix timestamp (days incl. fraction to seconds + unix offset 1.1.2000 12:00)
        *sunrise = (Jrise - 2451545.0) * 86400 + 946728000;  // OPTIONAL: Add an offset of +-seconds to the end of the line
        *sunset = (Jset - 2451545.0) * 86400 + 946728000;    // OPTIONAL: Add an offset of +-seconds to the end of the line
    }
}

#endif /*__SUN_H__*/
