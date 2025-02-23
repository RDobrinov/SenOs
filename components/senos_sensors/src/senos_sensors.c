/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_task.h"

#include "senos_bus_drv.h"
#include "senos_sensors.h"
#include "senos_sensor_private.h"
#include "ds18x20.h"
#include "bmx280.h"
#include "max31865.h"

static void vSenOSSensorTask(void *pvParameters);
//static void senos_sensor_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

const uint16_t senos_sensor_magnitude_divider[] = {1, 10, 100, 1000, 10000};
static const char *ccMagnitudeFormats[] = {"%.0f", "%.1f", "%.2f", "%.3f", "%.4f"};
const char *ccMagitudeName[] = {"", "Temperature", "Humidity", "Pressure", ""};
const char *ccMetricName[] = {"", "degrees Celsius", "degrees Fahrenheit", "kelvin", "Precents", "Pascals", "Hectopascals"};
const char *ccMetricSymbol[] = {"", "°C", "°F", "K", "%", "Pa", "hPa"};

static void *(*pvSenosBusCtrlHandle[4])(void) = {&ds18x20_get_interface, &bmx280_get_interface, &max31865_get_interface, NULL};
static senos_drv_bus_t _sensor2bus[3] = {SENOS_BUS_1WIRE, SENOS_BUS_I2C, SENOS_BUS_SPI};

typedef struct {
    uint32_t own_loop:1;
    uint32_t flags:31;
    senos_sensor_data_handle_t sensor_list;
    SemaphoreHandle_t sensor_lock;
    StaticSemaphore_t MutexBuffer;
    esp_event_loop_handle_t event_loop;
    TaskHandle_t task_handle;
} senos_sensor_task_config_t;

ESP_EVENT_DEFINE_BASE(SENSOR_EVENT);

static senos_sensor_task_config_t task_cfg = {};

/*
typedef struct senos_sensor {
    senos_sensor_meas_timing_t meas_timing;
    senos_sensor_timing_t state_timing;
    senos_sensor_caps_t caps;
    senos_sensor_handle_t *handle;
    struct senos_sensor *next;
} senos_sensor_data_t;
*/

static void vSenOSSensorTask(void *pvParameters) {
    //configTICK_RATE_HZ
    //vTaskDelay()
    /** За една секунда има 100 тика.  */
    esp_err_t err;
    TickType_t ticks;
    xSemaphoreTake(task_cfg.sensor_lock, portMAX_DELAY);
    senos_sensor_data_handle_t sensor = task_cfg.sensor_list;
    while(true) {
        if(sensor) {
            ticks = xTaskGetTickCount();
            if(sensor->state_timing.end < ticks) {
                switch(sensor->state_timing.state) {
                    case SENOS_SENSOR_INIT:
                        if(ESP_OK != (*sensor->handle)->_init(sensor->handle)) break;    /* Failed to initialize sensor */
                        if(ESP_OK != (*sensor->handle)->_getcaps(sensor->handle, &(sensor->caps))) break;
                        TickType_t meas_total_time = pdMS_TO_TICKS(sensor->caps.warmup_time + sensor->caps.prepare_time + sensor->caps.measure_time + sensor->caps.cooldown_time);
                        if(sensor->meas_conf.wait_for_next < meas_total_time) sensor->meas_conf.wait_for_next = (meas_total_time << 1);
                        /* Sensor must report time caps with failsafe delays. */
                        sensor->caps.warmup_time = pdMS_TO_TICKS(sensor->caps.warmup_time);
                        sensor->caps.prepare_time = pdMS_TO_TICKS(sensor->caps.prepare_time);
                        sensor->caps.measure_time = pdMS_TO_TICKS(sensor->caps.measure_time);
                        sensor->caps.cooldown_time = pdMS_TO_TICKS(sensor->caps.cooldown_time);
                        sensor->state_timing.state = SENOS_SENSOR_WAIT;
                        sensor->state_timing.end = sensor->meas_conf.wait_for_next + ticks;
                        break;
                    case SENOS_SENSOR_WAIT:
                        sensor->state_timing.state = SENOS_SENSOR_WARM;
                        if(sensor->caps.warmup_time) {
                            //printf("Run warmup\n");
                            sensor->state_timing.end = sensor->caps.warmup_time + ticks;
                            break;
                        }
                        __attribute__ ((fallthrough));
                    case SENOS_SENSOR_WARM:
                        sensor->state_timing.state = SENOS_SENSOR_PREPARE;
                        if(sensor->caps.prepare_time) {
                            //printf("Run prepare\n");
                            sensor->state_timing.end = sensor->caps.prepare_time + ticks;
                            break;
                        }
                        __attribute__ ((fallthrough));
                    case SENOS_SENSOR_PREPARE:
                        sensor->state_timing.state = SENOS_SENSOR_MEASURE;
                        //printf("Run measure\n");
                        (*sensor->handle)->_measure(sensor->handle);    //Check for errors.
                        sensor->state_timing.end = sensor->caps.measure_time + ticks;
                        break;
                    case SENOS_SENSOR_MEASURE:
                        sensor->state_timing.state = SENOS_SENSOR_READ;
                        //printf("Run read\n");
                        (*sensor->handle)->_read(sensor->handle);
                        break;
                    case SENOS_SENSOR_READ:
                        sensor->state_timing.state = SENOS_SENSOR_GET;
                        //printf("Run get\n");
                        (*sensor->handle)->_getvalue(sensor->handle, sensor->caps.mag_caps);
                        sensor->meas_conf.meas_count = (sensor->meas_conf.meas_count + 1) % sensor->meas_conf.report;
                        for(senos_sensor_mag_caps_t *sensor_mag_cap = sensor->caps.mag_caps; sensor_mag_cap != NULL; sensor_mag_cap = sensor_mag_cap->next) {
                            if(sensor_mag_cap->magnitude.valid && sensor->meas_conf.report > 1) {
                                if(sensor_mag_cap->magnitude.report_valid) {
                                    //printf("[SenOS_Sensor] %08lX mvalue %ld, rvalue %ld\n", (*sensor->handle)->_getid(sensor->handle), 
                                    //    (int32_t)sensor_mag_cap->magnitude.value, (int32_t)sensor_mag_cap->magnitude.report_value);
                                    int32_t iir_value = 25 * ((int32_t)sensor_mag_cap->magnitude.report_value - (int32_t)sensor_mag_cap->magnitude.value);
                                    iir_value += iir_value < 0 ? -50 : 50;
                                    sensor_mag_cap->magnitude.report_value = (uint32_t)((int32_t)sensor_mag_cap->magnitude.report_value - iir_value / 100);
                                    //sensor_mag_cap->magnitude.report_value = (uint32_t)((int32_t)sensor_mag_cap->magnitude.report_value 
                                    //    - ((int32_t)((0.25 * ((int32_t)sensor_mag_cap->magnitude.report_value - (int32_t)sensor_mag_cap->magnitude.value) * 100.0) + 0.5) / 100));
                                } else {
                                    sensor_mag_cap->magnitude.report_value = sensor_mag_cap->magnitude.value;
                                    sensor_mag_cap->magnitude.report_valid = true;
                                }
                            }
                            //printf("[SenOS_Sensor] %08lX Report value %f (report.valid: %d)\n", (*sensor->handle)->_getid(sensor->handle), (int32_t)(sensor_mag_cap->magnitude.report_value) / 
                            //        (float)senos_sensor_magnitude_divider[sensor_mag_cap->magnitude.decimals], sensor_mag_cap->magnitude.report_valid);
                        }
                        break;
                    case SENOS_SENSOR_GET:
                        sensor->state_timing.state = SENOS_SENSOR_COOL;
                        //printf("Post data\n");
                        //sprintf(format, "Measured %%.%df\n", magnitude->magnitude.decimals);
                        if(!sensor->meas_conf.meas_count) {
                            uint32_t id = (*sensor->handle)->_getid(sensor->handle);
                            for(senos_sensor_mag_caps_t *sensor_mag_cap = sensor->caps.mag_caps; sensor_mag_cap != NULL; sensor_mag_cap = sensor_mag_cap->next) {
                                printf("[%08lX] %s Report %s ", id, (*sensor->handle)->_getname(sensor->handle), ccMagitudeName[sensor_mag_cap->magnitude.type]);
                                if(sensor_mag_cap->magnitude.decimals) {
                                    printf(ccMagnitudeFormats[sensor_mag_cap->magnitude.decimals], sensor_mag_cap->magnitude.report_value / (float)senos_sensor_magnitude_divider[sensor_mag_cap->magnitude.decimals]);
                                }
                                else {
                                    printf("%ld", sensor_mag_cap->magnitude.report_value);
                                }
                                printf("%s (%s)\n", ccMetricSymbol[sensor_mag_cap->magnitude.metric], ccMetricName[sensor_mag_cap->magnitude.metric]);
                            }
                        }
                        if(sensor->caps.cooldown_time) {
                            //printf("Run cooldown\n");
                            sensor->state_timing.end = sensor->caps.cooldown_time + ticks;
                            break;
                        }
                        __attribute__ ((fallthrough));
                    case SENOS_SENSOR_COOL:
                        sensor->state_timing.state = SENOS_SENSOR_WAIT;
                        sensor->state_timing.end = sensor->meas_conf.wait_for_next + ticks;
                        __attribute__ ((fallthrough));
                    default:
                        break;
                }
            }
            xSemaphoreGive(task_cfg.sensor_lock);
            vTaskDelay(1);  /** Give some time */
            xSemaphoreTake(task_cfg.sensor_lock, portMAX_DELAY);
            sensor = sensor->next;
        } else {
            xSemaphoreGive(task_cfg.sensor_lock);
            vTaskDelay(1);  /** Give some time */
            xSemaphoreTake(task_cfg.sensor_lock, portMAX_DELAY);
            sensor = task_cfg.sensor_list;
        }
    }
}

esp_err_t fnSenosSensorScan(senos_sensor_hw_conf_t *dv, uint8_t *list, size_t *len) {
    if(dv->type != SENSOR_DS18X20) return ESP_ERR_NOT_SUPPORTED;
    senos_dev_cfg_t dc = {
        .bus_type = _sensor2bus[dv->type],
        .dev_1wire = {
            .data_gpio = dv->ds18x20.gpio
        }
    };
    return fnSenosBusScanBus(&dc, list, len);
}

esp_err_t fnSenosSensorAdd(senos_sensor_config_t *dv) {
    esp_err_t err;
    if(!task_cfg.sensor_lock) {
        err = fnSenosSensorInit();
        if(ESP_OK != err) return err;
    }
    if(!(*pvSenosBusCtrlHandle[dv->hw_cfg.type])) return ESP_ERR_NOT_SUPPORTED;
    senos_sensor_data_handle_t sensor = (senos_sensor_data_handle_t)calloc(1, sizeof(senos_sensor_data_t));
    if(!sensor) return ESP_ERR_NO_MEM;
    senos_sensor_interface_t interface = (senos_sensor_interface_t)(*pvSenosBusCtrlHandle[dv->hw_cfg.type])();
    if(!interface) return ESP_ERR_NOT_SUPPORTED;
    err =  interface->_add(&(dv->hw_cfg),&(sensor->handle));
    if(ESP_OK != err) return err;
    sensor->state_timing.state = SENOS_SENSOR_INIT;
    sensor->meas_conf.wait_for_next = (dv->timing.measure_interval) * configTICK_RATE_HZ;
    sensor->meas_conf.report = dv->timing.report_every;
    if(!sensor->meas_conf.report) sensor->meas_conf.report = 1;
    printf("%p->%p->%p\n", sensor, sensor->handle, *(sensor->handle));
    if(!xSemaphoreTake(task_cfg.sensor_lock, portMAX_DELAY)) {
        interface->_remove(&(sensor->handle));
        free(sensor);
        return ESP_ERR_TIMEOUT;
    }
    sensor->next = task_cfg.sensor_list;
    task_cfg.sensor_list = sensor;
    xSemaphoreGive(task_cfg.sensor_lock);
    return ESP_OK;
}
/*
static void senos_sensor_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    return;
}
*/

esp_err_t fnSenosSensorInit() {
    if(task_cfg.sensor_lock) return ESP_OK;
    esp_err_t err;
    esp_event_loop_args_t loop_args = {
        .queue_size = CONFIG_ESP_SYSTEM_EVENT_QUEUE_SIZE,
        .task_name = "os_sen_evt",
        .task_stack_size = ESP_TASKD_EVENT_STACK,
        .task_priority = ESP_TASKD_EVENT_PRIO,
        .task_core_id = 0
    };
    err = esp_event_loop_create(&loop_args, &task_cfg.event_loop);
    if(ESP_OK != err) return err;
    //esp_event_handler_instance_register_with(task_cfg.event_loop, SENSOR_EVENT, ESP_EVENT_ANY_ID, senos_sensor_event_handler, NULL, NULL);
    task_cfg.sensor_lock = xSemaphoreCreateMutexStatic(&task_cfg.MutexBuffer);
    xSemaphoreTake(task_cfg.sensor_lock, portMAX_DELAY);
    if(pdPASS != xTaskCreate(vSenOSSensorTask, "SenOS_Sensor", ESP_TASKD_EVENT_STACK, NULL, 15, &task_cfg.task_handle)) {
        esp_event_loop_delete(task_cfg.event_loop);
        task_cfg.event_loop = NULL;
        vSemaphoreDelete(task_cfg.sensor_lock);
        task_cfg.sensor_lock = NULL;
        return ESP_FAIL;
    }
    task_cfg.own_loop = true;
    xSemaphoreGive(task_cfg.sensor_lock);
    return ESP_OK;
}

esp_err_t fnSenosSensorGetEvtLoop(esp_event_loop_handle_t *evt_loop) {
    if(!task_cfg.sensor_lock) return ESP_ERR_INVALID_STATE;
    *evt_loop = task_cfg.event_loop;
    return ESP_OK;
}

esp_err_t fnSenosSensorSetEvtLoop(esp_event_loop_handle_t *evt_loop) {
    if(!task_cfg.sensor_lock) return ESP_ERR_INVALID_STATE;
    /** Suspend sensor task until event loop handle is changing */
    vTaskSuspend(task_cfg.task_handle);    
    if(task_cfg.own_loop) {
        esp_event_loop_delete(task_cfg.event_loop);
        task_cfg.own_loop = false;
    }
    task_cfg.event_loop = *evt_loop;
    return ESP_OK;
}
