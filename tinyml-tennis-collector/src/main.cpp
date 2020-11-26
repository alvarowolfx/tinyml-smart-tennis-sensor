#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <bluefruit.h>

BLEUart bleuart; // uart over ble
BLEDis bledis;   // device information
BLEDfu bledfu;   // OTA DFU service

#define MOTION_THRESHOLD 5.0
#define DATA_POINTS 60
#define RING_BUFFER_SIZE DATA_POINTS
#define WINDOW_MS 1000
#define DEFAULT_DELAY WINDOW_MS / (DATA_POINTS)
#define G_TO_MS2 9.80665f
#define CONTINUOUS_MODE 1
#define MOTION_DETECTION_MODE 2

Adafruit_MPU6050 mpu;

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

#define BLE_READING_SIZE sizeof(struct ble_reading)

struct ble_reading ringBuffer[RING_BUFFER_SIZE];
int ringBufferPointer = 0;

long start;
long lastTransmission;
double lastAbs;
int mode = MOTION_DETECTION_MODE;
//int mode = CONTINUOUS_MODE;

double abs_acceleration(double x, double y, double z)
{
  return sqrt(x * x + y * y + z * z);
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection *connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = {0};
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void)conn_handle;
  (void)reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
}

void addToRingBuffer(float ax, float ay, float az, float gx, float gy, float gz)
{
  ringBuffer[ringBufferPointer].ax = ax;
  ringBuffer[ringBufferPointer].ay = ay;
  ringBuffer[ringBufferPointer].az = az;
  ringBuffer[ringBufferPointer].gx = gx;
  ringBuffer[ringBufferPointer].gy = gy;
  ringBuffer[ringBufferPointer].gz = gz;
  ringBuffer[ringBufferPointer].pos = ringBufferPointer;
  ringBufferPointer++;
  if (ringBufferPointer >= RING_BUFFER_SIZE)
  {
    ringBufferPointer = 0;
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  if (!mpu.begin())
  {
    Serial.printf("Failed to initialize IMU!\r\n");
    while (1)
    {
      delay(10);
    }
  }
  else
  {
    Serial.printf("IMU initialized\r\n");
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("");
  delay(100);

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float lastX = a.acceleration.x;
  float lastY = a.acceleration.y;
  float lastZ = a.acceleration.z;
  lastAbs = abs_acceleration(lastX, lastY, lastZ);

  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Tennis Sensor");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Alvaro Viebrantz");
  bledis.setModel("Mark One");
  bledis.begin();

  bleuart.begin();

  // Set up and start advertising
  startAdv();

  lastTransmission = millis();
  start = millis();
}

void loop()
{
  bool motion = false;
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float curX = a.acceleration.x;
  float curY = a.acceleration.y;
  float curZ = a.acceleration.z;
  float gx = g.gyro.x;
  float gy = g.gyro.y;
  float gz = g.gyro.z;

  if (mode == MOTION_DETECTION_MODE)
  {
    double absAcc = abs_acceleration(curX, curY, curZ);
    double absAccDiff = abs(absAcc - lastAbs);
    if (absAccDiff > MOTION_THRESHOLD)
    {
      Serial.print("Motion detected!!! - ");
      Serial.print(absAcc);
      Serial.print(" , ");
      Serial.print(absAccDiff);
      Serial.println();
      if (millis() - lastTransmission <= 1000)
      {
        Serial.print("Multiple Motion detected!!!");
      }
      else
      {
        motion = true;
      }
    }
    else
    {
      /*Serial.print("No Motion - ");
      Serial.print(diffX);
      Serial.print(" , ");
      Serial.print(diffY);
      Serial.print(" , ");
      Serial.print(diffZ);
      Serial.print(", DIFF - ");
      Serial.print(diffX + diffY + diffZ);
      Serial.print(",");
      Serial.print(" , ");
      Serial.print(curX);
      Serial.print(" , ");
      Serial.print(curY);
      Serial.print(" , ");
      Serial.print(curZ);
      Serial.println();*/
      delay(DEFAULT_DELAY);
    }
  }

  lastAbs = abs_acceleration(curX, curY, curZ);

  addToRingBuffer(curX, curY, curZ, gx, gy, gz);

  if (mode == MOTION_DETECTION_MODE)
  {
    if (motion)
    {
      int readings = 0;
      int ringBufferCurrentPos = ringBufferPointer - DATA_POINTS / 2;
      if (ringBufferCurrentPos < 0)
      {
        ringBufferCurrentPos += RING_BUFFER_SIZE;
      }
      Serial.println("Transmitting");
      start = millis();
      while (readings < DATA_POINTS / 2)
      {
        /*Serial.print(curX);
        Serial.print(" , ");
        Serial.print(curY);
        Serial.print(", ");
        Serial.print(curZ);
        Serial.println();*/

        readings++;
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        curX = a.acceleration.x;
        curY = a.acceleration.y;
        curZ = a.acceleration.z;
        gx = g.gyro.x;
        gy = g.gyro.y;
        gz = g.gyro.z;
        addToRingBuffer(curX, curY, curZ, gx, gy, gz);
        delay(DEFAULT_DELAY);
      }

      for (int i = 0; i < RING_BUFFER_SIZE; i++)
      {
        int pos = ringBufferCurrentPos + i;
        if (pos >= RING_BUFFER_SIZE)
        {
          pos -= RING_BUFFER_SIZE;
        }
        bleuart.write((const unsigned char *)&ringBuffer[pos], BLE_READING_SIZE);
        delay(DEFAULT_DELAY);
      }

      Serial.print("Finished transmission - ");
      Serial.print(millis() - start);
      Serial.println(" ms");

      lastAbs = abs_acceleration(curX, curY, curZ);
    }
  }
  else
  {
    bleuart.write((const unsigned char *)&ringBuffer[ringBufferPointer], BLE_READING_SIZE);
    delay(DEFAULT_DELAY);
  }
}
