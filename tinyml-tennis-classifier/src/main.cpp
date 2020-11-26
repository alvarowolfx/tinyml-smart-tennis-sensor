#include <Arduino.h>
#include <tennis-sensor_inference.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#define CONVERT_G_TO_MS2 9.80665f
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
Adafruit_MPU6050 mpu;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Edge Impulse Inferencing Demo");
  pinMode(LED_RGB_RED, OUTPUT);
  pinMode(LED_RGB_GREEN, OUTPUT);
  pinMode(LED_RGB_BLUE, OUTPUT);

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

  if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3)
  {
    Serial.printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 3 (the 3 sensor axes)\n");
    return;
  }
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop()
{
  //ei_printf("\nStarting inferencing in 2 seconds...\n");
  //delay(2000);

  Serial.printf("Sampling...\n");

  // Allocate a buffer here for the values we'll read from the IMU
  float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
  {
    // Determine the next tick (and then sleep later)
    uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    /*Serial.printf("ax %f ay %f ax %f gx %f gy %f gz %f \n",
                  a.acceleration.x,
                  a.acceleration.y,
                  a.acceleration.z,
                  g.gyro.x,
                  g.gyro.y,
                  g.gyro.z);*/

    buffer[ix] = a.acceleration.x;
    buffer[ix + 1] = a.acceleration.y;
    buffer[ix + 2] = a.acceleration.z;
    buffer[ix + 3] = g.gyro.x;
    buffer[ix + 4] = g.gyro.y;
    buffer[ix + 5] = g.gyro.z;

    delayMicroseconds(next_tick - micros());
  }

  // Turn the raw buffer in a signal which we can the classify
  signal_t signal;
  int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0)
  {
    Serial.printf("Failed to create signal from buffer (%d)\n", err);
    return;
  }

  // Run the classifier
  ei_impulse_result_t result = {0};

  err = run_classifier(&signal, &result, debug_nn);
  if (err != EI_IMPULSE_OK)
  {
    Serial.printf("ERR: Failed to run classifier (%d)\n", err);
    return;
  }

  // print the predictions
  Serial.printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);
  int labelIndex = 0;
  float maxValue = -1.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
  {
    Serial.printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    if (result.classification[ix].value > maxValue)
    {
      maxValue = result.classification[ix].value;
      labelIndex = ix;
    }
  }

  digitalWrite(LED_RGB_RED, 255);
  digitalWrite(LED_RGB_GREEN, 255);
  digitalWrite(LED_RGB_BLUE, 255);
  if (labelIndex == 0 && maxValue > 0.75)
  { //Backhand
    Serial.printf("Backhand!!!");
    digitalWrite(LED_RGB_BLUE, 0);
    delay(300);
  }
  else if (labelIndex == 1 && maxValue > 0.75)
  { //Forehand
    Serial.printf("Forehand!!!");
    digitalWrite(LED_RGB_GREEN, 0);
    delay(300);
  }
  else
  { //Idle
    Serial.printf("idle!!!");
    digitalWrite(LED_RGB_RED, 0);
  }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
  Serial.printf("    anomaly score: %.3f\n", result.anomaly);
#endif
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_ACCELEROMETER
#error "Invalid model for current sensor"
#endif
