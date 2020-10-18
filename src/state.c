#include <zephyr/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "state.h"

static struct ble_reading current_ble_reading = {};

static bool is_transmiting = false;
static int transmissions = 0;

void start_transmission()
{
  is_transmiting = true;
  transmissions = 0;
}

void stop_transmission()
{
  is_transmiting = false;
  transmissions = DATA_POINTS_PER_WINDOW;
}

void increment_transmissions()
{
  transmissions++;
  if (transmissions > DATA_POINTS_PER_WINDOW)
  {
    is_transmiting = false;
  }
}

bool is_transmitting()
{
  return is_transmiting;
  //return true;
}

bool should_transmit()
{
  return transmissions <= DATA_POINTS_PER_WINDOW;
  //return true;
}

void update_sensor(struct acc_reading reading, int pos)
{
  current_ble_reading.ax = reading.ax;
  current_ble_reading.ay = reading.ay;
  current_ble_reading.az = reading.az;
  current_ble_reading.gx = reading.gx;
  current_ble_reading.gy = reading.gy;
  current_ble_reading.gz = reading.gz;
  current_ble_reading.pos = pos;
}

struct ble_reading get_ble_reading()
{
  return current_ble_reading;
}
