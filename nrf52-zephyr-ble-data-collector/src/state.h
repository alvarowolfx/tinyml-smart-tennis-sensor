#ifndef __STATE_H__
#define __STATE_H__

#include <kernel.h>
#include <data/json.h>

#define DATA_POINTS_PER_WINDOW 60
#define TX_SLEEP_TIME K_MSEC(16)

struct acc_reading
{
  //void *fifo_reserved; /* 1st word reserved for use by fifo */
  double ax;
  double ay;
  double az;
  double gx;
  double gy;
  double gz;
};

struct ble_reading
{
  double ax;
  double ay;
  double az;
  double gx;
  double gy;
  double gz;
  int pos;
};

#define acc_reading_size_t sizeof(struct acc_reading)
#define reading_size_t sizeof(struct ble_reading)

#ifdef CONFIG_JSON_LIBRARY
static const struct json_obj_descr ble_reading_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct ble_reading, ax, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct ble_reading, ay, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct ble_reading, az, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct ble_reading, gx, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct ble_reading, gy, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct ble_reading, gz, JSON_TOK_NUMBER),
};
#endif

void update_sensor(struct acc_reading reading, int pos);

void start_transmission();
void stop_transmission();

void increment_transmissions();
bool is_transmitting();
bool should_transmit();
struct ble_reading get_ble_reading();

#endif