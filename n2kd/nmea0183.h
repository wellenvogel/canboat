/*

Convert JSON encoded NMEA2000 PGNs to NMEA0183.

At this moment it only supports what one of the authors needed: sensor data
other than GPS and AIS: depth, heading, wind.

(C) 2009-2013, Kees Verruijt, Harlingen, The Netherlands.

This file is part of CANboat.

CANboat is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

CANboat is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with CANboat.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef _NMEA0183_H
#define _NMEA0183_H

#include "common.h"

extern void convertJSONToNMEA0183(StringBuffer *msg183, const char *msg);
extern void nmea0183CreateMessage(StringBuffer *msg183, int src, const char *format, ...);

/*
 * NMEA 2000 is all ISO units, so m/s, deg K, etc. except for angle and angular momentum which
 * are in rad and rad/s or deg and deg/s.
 *
 * NMEA 0183 uses various units, including metric derived and colonial.
 */
#define SPEED_M_S_TO_KNOTS(s) (s * 1.94384)
#define SPEED_M_S_TO_KMH(s) ((s) *3.6)
#define DIST_M_TO_KM(d) ((d) / 1000.0)
#define DIST_M_TO_NM(d) ((d) / 1852.0)

#define TEMP_K_TO_C(t) (((t) -273.15))

typedef enum
{
  U_ANGLE,
  U_ANGULAR_VELOCITY,
  U_SPEED,
  U_DISTANCE,
  U_TEMPERATURE,
  U_MAX
} Unit;

// List of conversion values for each unit. When analyser produces SI convert it to 'human standard'.
static double UNIT_CONVERSION[U_MAX][2] = {{1, RadianToDegree}, {1, RadianToDegree}, {1, 1}, {1, 1}, {1, 1}};
static double UNIT_OFFSET[U_MAX][2]     = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, -273.15}};

extern bool extractNumber(const char *message, const char *fieldName, double *value, Unit unit);
extern bool extractInteger(const char *message, const char *fieldName, int *value);

#endif
