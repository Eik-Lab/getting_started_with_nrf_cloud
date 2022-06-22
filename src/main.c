/*
 * Copyright (c) 2022 Lars Øvergård
 *
 * Copyright (c) 2020 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file main.c
 *
 * @brief Firmware for collecting sensor data and sending it to a cloud
 * provider.
 */

// ---------Includes-----------
// Zephyr
#include <zephyr.h>
// Prints/Logs
#include <stdio.h>
// LTE and Cloud
#include <modem/lte_lc.h>
#include <net/cloud.h>
// Buttons and leds
#include <dk_buttons_and_leds.h>
// Reboot
#include <sys/reboot.h>
// Timestamp
#include <date_time.h>
// Watchdog
#include <watchdog.h>
// Logging
#include <logging/log.h>
// Battery
#include <modem/modem_info.h>

// Sensor lib and device drivers
#include <device.h>
// Uncomment the section bellow for I2C
// #include <drivers/i2c.h>
// #include <nrfx_twim.h>

// Library that contains useful structs for sensor data/ sensors/ sensors
// #include <drivers/sensor.h>

/* Sensor unit, not implemented */
#if defined(CONFIG_SENSOR_USAGE)
// ****************************************************************************
// PUT LIBRARIES THAT YOU WANT TO USE HERE
// ****************************************************************************
#endif
// ----------------------------

// Struct that holds data from the modem.
static struct modem_param_info modem_param;
// Timestamp
static int64_t date_time_stamp;
// Struct that holds data from the cloud backend information.
static struct cloud_backend *cloud_backend;

// NRFCloud JSON & UI
#define SERVICE_INFO_SENSORS \
  "{\"state\":{\"reported\":{\"device\": \
			  {\"serviceInfo\":{\"ui\":[\"HUMIDITY\"]}}}}}"

LOG_MODULE_REGISTER(WATER_QA, CONFIG_WATER_QA_LOG_LEVEL);

/* Stack definition for application workqueue */
K_THREAD_STACK_DEFINE(application_stack_area,
                      CONFIG_APPLICATION_WORKQUEUE_STACK_SIZE);
/* Struct work queue */
static struct k_work_q application_work_q;

// ----------Work-----------
/* Struct work for connecting to the cloud */
static struct k_work_delayable cl_reconnect;
/* Struct work for rebooting the device */
static struct k_work_delayable reboot_work;
/* Struct work for starting data sampling */
static struct k_work_delayable start_sampling;
// -------------------------

/**
 * @brief Reads the battery voltage, eiter from a voltage divider
 * or the modem
 */
static int batt_measure(void) {
  // Sets the default value to -1
  int batt_mV = -1;
  int err;
  err = modem_info_params_get(&modem_param);
  if (err) {
    LOG_ERR("Could not get modem info, error: %d", err);
    return err;
  } else {
    batt_mV = modem_param.device.battery.value;
  }

  return batt_mV;
}

/**
 * @brief Polls the data from the I2C load cell, not working!
 */
static void sample_data(double *sens_data) {
  // Template function for sampling data.
  // TODO: Change this to suit your usecase
  *sens_data = 10;

#if defined(CONFIG_SENSOR_USAGE)
  /*
  Poll sensor data
  */
#endif

  LOG_INF("Data: %f", *sens_data);
}

/**
 * @brief Creates a message, which will send data,
 * reports data that is enabled
 */
static void send_data(void) {
  // Template function for sending data.
  // TODO: Change this to include your data from your usecase. See battery
  // function for inspiration.
  int err;
  char buf[150];
  struct cloud_msg msg = {
      .qos = CLOUD_QOS_AT_MOST_ONCE,
      .endpoint.type = CLOUD_EP_MSG,
      .buf = buf,
  };
  int battery_level = batt_measure();
  double data_sample;
  sample_data(&data_sample);
  date_time_now(&date_time_stamp);

  msg.len = snprintf(buf, sizeof(buf),
                     "{"
                     "\"appId\":\"HUMIDITY\","
                     "\"data\":\"%f\","
                     "\"battery\":\"%d\","
                     "\"timestamp\":\"%i%i\""
                     "}",
                     data_sample, battery_level,
                     (uint32_t)(date_time_stamp / 1000000),
                     (uint32_t)(date_time_stamp % 1000000));
  if (msg.len < 0) {
    LOG_ERR("Failed to create data cloud message");
    return;
  }

  err = cloud_send(cloud_backend, &msg);
  if (err) {
    LOG_ERR("Failed to send data message to cloud, error: %d", err);
    return;
  }

  LOG_INF("Data sent to cloud");
}

/**
 * @brief Starts sampling data and call the send_data func.
 */
static void start_sampling_fn(struct k_work *work) {
  ARG_UNUSED(work);

  // Will set a better way to sample at 10Hz later
  while (true) {
    send_data();
    k_sleep(K_MSEC(20000));
  }
}

/**
 * @brief Reconnects to the cloud and it will start
 * sending data.
 */
static void cl_reconnect_fn(struct k_work *work) {
  int err;
  ARG_UNUSED(work);

  err = cloud_connect(cloud_backend);
  if (err) {
    LOG_ERR("Cloud connection failed, error: %d", err);
    return;
  }
}

/**
 * @brief Tells nrfcloud it has capabilities it,
 * so the UI will be automatically setup up with
 * the correct layout
 */
static void send_service_info(void) {
  int err;
  struct cloud_msg msg = {.qos = CLOUD_QOS_AT_MOST_ONCE,
                          .endpoint.type = CLOUD_EP_STATE,
                          .buf = SERVICE_INFO_SENSORS,
                          .len = strlen(SERVICE_INFO_SENSORS)};

  err = cloud_send(cloud_backend, &msg);
  if (err) {
    LOG_ERR("Failed to send message to cloud, error: %d", err);
    return;
  }

  LOG_INF("Service info sent to cloud");
}

/**
 * @brief Cloud event handler
 */
static void cloud_event_handler(const struct cloud_backend *const backend,
                                const struct cloud_event *const evt,
                                void *user_data) {
  ARG_UNUSED(backend);
  ARG_UNUSED(user_data);

  switch (evt->type) {
    case CLOUD_EVT_CONNECTING:
      LOG_INF("CLOUD_EVT_CONNECTING");
      break;
    case CLOUD_EVT_CONNECTED:
      LOG_INF("CLOUD_EVT_CONNECTED");
      break;
    case CLOUD_EVT_READY:
      LOG_INF("CLOUD_EVT_READY");
      /* Update nRF Cloud with service GUI*/
      send_service_info();

      // Starts sampling
      k_work_reschedule_for_queue(&application_work_q, &start_sampling,
                                  K_NO_WAIT);
      break;
    case CLOUD_EVT_DISCONNECTED:
      LOG_INF("CLOUD_EVT_DISCONNECTED");
      break;
    case CLOUD_EVT_ERROR:
      LOG_INF("CLOUD_EVT_ERROR");
      break;
    case CLOUD_EVT_DATA_SENT:
      LOG_INF("CLOUD_EVT_DATA_SENT");
      break;
    case CLOUD_EVT_DATA_RECEIVED:
      LOG_INF("CLOUD_EVT_DATA_RECEIVED");

      /* Convenience functionality for remote testing.
       * The device is reset if it receives "{"reboot":true}"
       * from the cloud. The command can be sent using the terminal
       * card on the device page on nrfcloud.com.
       */
      if (evt->data.msg.buf[0] == '{') {
        if (strncmp(evt->data.msg.buf, "{\"reboot\":true}",
                    strlen("{\"reboot\":true}")) == 0) {
          LOG_INF("Reboot received");

          /* The work item may already be scheduled
           * because of the button being pressed,
           * so use rescheduling here to get the work
           * submitted with no delay.
           */
          k_work_reschedule_for_queue(&application_work_q, &reboot_work,
                                      K_NO_WAIT);
          break;
        }
        break;
      }
      break;
    case CLOUD_EVT_PAIR_REQUEST:
      LOG_INF("CLOUD_EVT_PAIR_REQUEST");
      break;
    case CLOUD_EVT_PAIR_DONE:
      LOG_INF("CLOUD_EVT_PAIR_DONE");
      break;
    case CLOUD_EVT_FOTA_DONE:
      LOG_INF("CLOUD_EVT_FOTA_DONE");
      break;
    case CLOUD_EVT_FOTA_ERROR:
      LOG_INF("CLOUD_EVT_FOTA_ERROR");
      break;
    default:
      LOG_INF("Unknown cloud event type: %d", evt->type);
      break;
  }
}

/**
 * @brief Reboots the device
 */
static void reboot_work_fn(struct k_work *work) {
  ARG_UNUSED(work);

  LOG_WRN("Rebooting in 2 seconds...");
  k_sleep(K_SECONDS(2));
  sys_reboot(0);
}

/**
 * @brief Initizalies all the work functions
 */
static void work_init(void) {
  k_work_init_delayable(&cl_reconnect, cl_reconnect_fn);
  k_work_init_delayable(&reboot_work, reboot_work_fn);
  k_work_init_delayable(&start_sampling, start_sampling_fn);
}

/**
 * @brief Configures the modem
 */
static int modem_configure(void) {
  int err = 0;

  if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
    /* Do nothing, modem is already turned on
     * and connected.
     */
  } else {
    LOG_INF("Connecting to LTE network. This may take minutes.");

#if defined(CONFIG_LTE_POWER_SAVING_MODE)
    err = lte_lc_psm_req(true);
    if (err) {
      LOG_ERR("PSM request failed, error: %d", err);
      return err;
    }

    LOG_INF("PSM mode requested");
#endif

    err = lte_lc_init_and_connect();
    if (err) {
      LOG_ERR("LTE link could not be established, error: %d", err);
      return err;
    }

    LOG_INF("Connected to LTE network");
  }

  return err;
}

#if defined(CONFIG_SENSOR_USAGE)
/**
 * @brief Initizalies the sensor
 */
static int sensor_device_init(void) {
  // Initialize your sensor(s) here.
  // TODO: Change this to your sensor(s) initialization code.
  int err;
  sensor_dev = device_get_binding(CONFIG_SENSOR_DEV_NAME);

  if (sensor_dev == NULL) {
    LOG_ERR("Could not get %s device", log_strdup(CONFIG_SENSOR_DEV_NAME));
  }

  return err;
}
#endif

/**
 * @brief Button handler
 */
static void button_handler(uint32_t button_states, uint32_t has_changed) {
  if (has_changed & button_states & DK_BTN1_MSK) {
    send_data();
    k_work_schedule_for_queue(&application_work_q, &reboot_work, K_SECONDS(5));
  } else if (has_changed & ~button_states & DK_BTN1_MSK) {
    k_work_cancel_delayable(&reboot_work);
  }
}

/**
 * @brief Datetime event handler
 */
static void date_time_event_handler(const struct date_time_evt *evt) {
  switch (evt->type) {
    case DATE_TIME_OBTAINED_MODEM:
      LOG_INF("DATE_TIME_OBTAINED_MODEM");
      break;
    case DATE_TIME_OBTAINED_NTP:
      LOG_INF("DATE_TIME_OBTAINED_NTP");
      break;
    case DATE_TIME_OBTAINED_EXT:
      LOG_INF("DATE_TIME_OBTAINED_EXT");
      break;
    case DATE_TIME_NOT_OBTAINED:
      LOG_INF("DATE_TIME_NOT_OBTAINED");
      break;
    default:
      break;
  }
}

void main(void) {
  int err;

  k_work_queue_start(&application_work_q, application_stack_area,
                     K_THREAD_STACK_SIZEOF(application_stack_area),
                     CONFIG_APPLICATION_WORKQUEUE_PRIORITY, NULL);
  if (IS_ENABLED(CONFIG_WATCHDOG)) {
    watchdog_init_and_start(&application_work_q);
  }

  LOG_INF("Water quality sampling has started");

  cloud_backend = cloud_get_binding("NRF_CLOUD");
  __ASSERT(cloud_backend, "Could not get binding to cloud backend");

  err = cloud_init(cloud_backend, cloud_event_handler);
  if (err) {
    LOG_ERR("Cloud backend could not be initialized, error: %d", err);
    return;
  }

  work_init();

  err = modem_configure();
  if (err) {
    LOG_ERR("Modem configuration failed with error %d", err);
    return;
  }

  err = modem_info_init();
  if (err) {
    LOG_INF("Could not initialize modem info, error: %d", err);
    return;
  }

  err = modem_info_params_init(&modem_param);
  if (err) {
    LOG_INF("Could not initialize modem info params, error: %d", err);
    return;
  }

#if defined(CONFIG_SENSOR_USAGE)
  err = sensor_device_init();
  if (err) {
    LOG_INF("Could not initialize load cell, error: %d", err);
    return;
  }
#endif

  err = dk_buttons_init(button_handler);
  if (err) {
    LOG_ERR("Buttons could not be initialized, error: %d", err);
    LOG_WRN("Continuing without button funcitonality");
  }

  date_time_update_async(date_time_event_handler);

  err = cloud_connect(cloud_backend);
  if (err) {
    LOG_ERR("Cloud connection failed, error: %d", err);
    return;
  }
}
