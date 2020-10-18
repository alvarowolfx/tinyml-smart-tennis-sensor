#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>
#include <zephyr/types.h>
#include <usb/usb_device.h>
#include <stddef.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include "gatt_nus_service.h"

#include "state.h"
#define READING_BUFFER_SIZE DATA_POINTS_PER_WINDOW

struct acc_reading readings[READING_BUFFER_SIZE];
int cur_reading_pos = 0;

/* size of stack area used by each thread */
#define ACCEL_STACKSIZE 1024
#define BLE_STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

static const struct bt_data ad[] = {BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR), BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E, )};

static u8_t is_connected = 0;

static void connected(struct bt_conn *conn, u8_t err)
{
  if (err)
  {
    printk("Connection failed (err %u)\n", err);
    is_connected = 0;
  }
  else
  {
    printk("Connected\n");
    is_connected = 1;
  }
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
  printk("Disconnected (reason %u)\n", reason);
  is_connected = 0;
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static void bt_ready(int err)
{
  if (err)
  {
    printk("Bluetooth init failed (err %d)\n", err);
    return;
  }

  printk("Bluetooth initialized\n");

  gatt_nus_service_init();

  /* Start advertising */
  err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad),
                        NULL, 0);
  if (err)
  {
    printk("Advertising failed to start (err %d)\n", err);
    return;
  }

  printk("Gatt service started\n");
}

static void button_pressed(struct device *dev, struct gpio_callback *cb, gpio_port_pins_t pins)
{
  printk("Pressed \n");
  if (is_transmitting())
  {
    stop_transmission();
  }
  else
  {
    k_sleep(K_MSEC(1000));
    start_transmission();
  }
}

static void configure_button(void)
{
  static struct gpio_callback button_cb;

  struct device *gpio = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(sw0), gpios));

  gpio_pin_configure(gpio, DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
                     GPIO_INPUT | DT_GPIO_FLAGS(DT_ALIAS(sw0), gpios));
  gpio_pin_interrupt_configure(gpio, DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
                               GPIO_INT_EDGE_TO_ACTIVE);

  gpio_init_callback(&button_cb, button_pressed,
                     BIT(DT_GPIO_PIN(DT_ALIAS(sw0), gpios)));

  gpio_add_callback(gpio, &button_cb);
}

void main(void)
{
  int err;

  printk("Initializing...\n");

  /* Initialize the Bluetooth Subsystem */
  err = bt_enable(bt_ready);
  if (err)
  {
    printk("Bluetooth init failed (err %d)\n", err);
  }

  bt_conn_cb_register(&conn_callbacks);

  configure_button();
}

void reading_put(struct acc_reading reading)
{
  readings[cur_reading_pos] = reading;
  cur_reading_pos++;
  if (cur_reading_pos > READING_BUFFER_SIZE)
  {
    cur_reading_pos = 0;
  }
}

void accel_task(void)
{
  struct sensor_value accel[3];
  struct sensor_value gyro[3];
  struct device *dev = device_get_binding(DT_LABEL(DT_INST(0, invensense_mpu6050)));

  if (dev == NULL)
  {
    printf("Could not get mpu6050 device\n");
    return;
  }

  s64_t start = k_uptime_get();
  int start_reading_pos = -1;
  bool stopped = true;
  while (1)
  {
    sensor_sample_fetch(dev);
    k_sleep(TX_SLEEP_TIME);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);

    struct acc_reading last_reading = {
        .ax = sensor_value_to_double(&accel[0]),
        .ay = sensor_value_to_double(&accel[1]),
        .az = sensor_value_to_double(&accel[2]),
        .gx = sensor_value_to_double(&gyro[0]),
        .gy = sensor_value_to_double(&gyro[1]),
        .gz = sensor_value_to_double(&gyro[2]),
    };
    reading_put(last_reading);

    if (is_transmitting())
    {
      if (stopped == true)
      {
        start = k_uptime_get();
        stopped = false;
        start_reading_pos = cur_reading_pos;
      }
      if (should_transmit())
      {
        increment_transmissions();
      }
    }
    else if (stopped == false)
    {
      stop_transmission();
      stopped = true;
      s64_t milliseconds_spent;
      milliseconds_spent = k_uptime_delta(&start);

      for (int i = 0; i < READING_BUFFER_SIZE; i++)
      {
        int pos = start_reading_pos + i;
        if (pos >= READING_BUFFER_SIZE)
        {
          pos -= READING_BUFFER_SIZE;
        }
        struct acc_reading *rx_data = &readings[pos];

        printk("Sending over ble \n");
        update_sensor(*rx_data, i);

        s64_t start = k_uptime_get();
        gatt_nus_service_data_notify(NULL);
        s64_t milliseconds_spent;
        milliseconds_spent = k_uptime_delta(&start);
        printk("%lld ms to transmit \n", milliseconds_spent);
      }

      printk("ended transmission - collected data for ");
      printk("%lld ms \n", milliseconds_spent);
    }
  }
}

K_THREAD_DEFINE(accel_task_id, ACCEL_STACKSIZE, accel_task, NULL, NULL, NULL, PRIORITY, 0, 0);