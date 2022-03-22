/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "driver/adc.h"
#include "dht.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "my_wifi_station.h"
#include "optmed.h"

#define STORAGE_NAMESPACE "storage"

static const char *TAG = "MQTT_EXAMPLE";

/* LOCAL GLOBALS = BAD */
static bool mqtt_connected = false;  // Indicates if MQTT is connected to broker
static bool enable_pump = true;      // If false, pump will not operate (for testing)
static bool use_fake_poll = false;   // If true, uses the following fake data during polling (for testing)
static uint32_t fake_moisture = 0;   // fake moisture value to return during polling (for testing)
static uint32_t fake_level = 0;      // fake level value to return during polling (for testing)

#define MOISTURE_SENSOR_DRY 720      // Sensor value from calibration - read while sensor dry and in air
#define MOISTURE_SENSOR_WET 2616     // Sensor value from calibration - read while sensor wet and in a glass of water
#define SEC_IN_MICROSEC 1000000ull   // Conversion factor
#define PLANT_NVS_KEY "plant"

#define MOISTURE_SENSOR_VALUE_FROM_RATIO(x) (x * (MOISTURE_SENSOR_WET - MOISTURE_SENSOR_DRY) + MOISTURE_SENSOR_DRY)
#define RATIO_FROM_MOISTURE_SENSOR_VALUE(x) ((x - MOISTURE_SENSOR_DRY) / ((float) (MOISTURE_SENSOR_WET - MOISTURE_SENSOR_DRY)))

enum PlantStates{
    PLANT_DRYING = 0,
    PLANT_PUMP_DELAY = 1,
    PLANT_PUMP_ON = 2,
    PLANT_WET_HOLD = 3,
    PLANT_DRY_HOLD = 4,
    PLANT_ALARM = 5
};

const char* PlantStateString[] = {
    "DRYING",
    "PUMP_DELAY",
    "PUMP_ON",
    "WET_HOLD",
    "DRY_HOLD",
    "ALARM"
};


// GPIO and ADC Pin configurations for plant
struct plant_pin_config_struct{
    adc1_channel_t moisture_sensor_adc1_channel; 
    adc1_channel_t level_sensor_adc1_channel;
    gpio_num_t pump_gpio_pin;
    gpio_num_t dht_gpio_pin;
};

// Watering Algorithm Parameters for plant
struct plant_watering_config_struct{
    uint16_t low_moisture;
    uint16_t watered_moisture;
    uint16_t high_moisture;
    uint16_t polling_period_s;
    uint16_t pump_on_period_s;
    uint16_t pump_off_period_s;
    uint16_t wet_hold_period_s;
    uint16_t dry_hold_period_s;
};

// Plant State and Status info
struct plant_status_struct{
    uint16_t poll_median_moisture_sensor;
    uint16_t poll_median_level_sensor;
    float poll_temperature;
    float poll_humidity;
    uint64_t state_entry_time_us;
    uint64_t last_poll_time_us;
    enum PlantStates state;
    bool initialized;
};

// All plant parameters
struct plant_struct{
    struct plant_pin_config_struct pins;
    struct plant_watering_config_struct config;
    struct plant_status_struct status;
};

void print_plant_pin_config_struct(const struct plant_pin_config_struct *plant_pins, const char *prefix){
    printf("%smoisture_sensor_adc1_channel = %d\n", prefix, plant_pins->moisture_sensor_adc1_channel);
    printf("%slevel_sensor_adc1_channel    = %d\n", prefix, plant_pins->level_sensor_adc1_channel);
    printf("%spump_gpio_pin                = %d\n", prefix, plant_pins->pump_gpio_pin);
    printf("%sdht_gpio_pin                 = %d\n", prefix, plant_pins->dht_gpio_pin);
}

void print_plant_watering_config_struct(const struct plant_watering_config_struct *watering_config, const char *prefix){
    printf("%slow_moisture      = %d\n", prefix, watering_config->low_moisture);
    printf("%swatered_moisture  = %d\n", prefix, watering_config->watered_moisture);
    printf("%shigh_moisture     = %d\n", prefix, watering_config->high_moisture);
    printf("%swatered_moisture  = %d\n", prefix, watering_config->watered_moisture);
    printf("%spolling_period_s  = %d\n", prefix, watering_config->polling_period_s);
    printf("%spump_on_period_s  = %d\n", prefix, watering_config->pump_on_period_s);
    printf("%spump_off_period_s = %d\n", prefix, watering_config->pump_off_period_s);
    printf("%swet_hold_period_s = %d\n", prefix, watering_config->wet_hold_period_s);
    printf("%sdry_hold_period_s = %d\n", prefix, watering_config->dry_hold_period_s);
}

void print_plant_status_struct(const struct plant_status_struct *status, const char *prefix){
    printf("%spoll_median_moisture_sensor = %d\n", prefix, status->poll_median_moisture_sensor);
    printf("%spoll_median_level_sensor    = %d\n", prefix, status->poll_median_level_sensor);
    printf("%spoll_temperature            = %0.1f\n", prefix, status->poll_temperature);
    printf("%spoll_humidity               = %0.1f\n", prefix, status->poll_humidity);
    printf("%sstate_entry_time_us         = %llu\n", prefix, status->state_entry_time_us);
    printf("%slast_poll_time_us           = %llu\n", prefix, status->last_poll_time_us);
    printf("%sstate                       = %d (%s)\n", prefix, status->state, PlantStateString[status->state]);
    printf("%sinitialized                 = %d\n", prefix, status->initialized);
}

void print_plant_struct(const struct plant_struct *plant){
    printf("Plant Struct:\n");
    printf("  Pin Config:\n");
    print_plant_pin_config_struct(&plant->pins, "    ");
    printf("  Watering Config:\n");
    print_plant_watering_config_struct(&plant->config, "    ");
    printf("  Status:");
    print_plant_status_struct(&plant->status, "    ");
}

const struct plant_status_struct plant_status_struct_default = {
    // State values
    .poll_median_moisture_sensor = 0,
    .poll_median_level_sensor = 0,
    .state_entry_time_us = 0,
    .last_poll_time_us = 0,
    .state = PLANT_DRYING,
    .initialized = false
};

// Default plant values
const struct plant_struct plant_default = {
    // Configuration values
    .pins = {
        .moisture_sensor_adc1_channel = ADC1_CHANNEL_4,
        .level_sensor_adc1_channel = ADC1_CHANNEL_5,
        .pump_gpio_pin = GPIO_NUM_18,
        .dht_gpio_pin = GPIO_NUM_19
    },
    .config = {
        .low_moisture = MOISTURE_SENSOR_VALUE_FROM_RATIO(.80),
        .watered_moisture = MOISTURE_SENSOR_VALUE_FROM_RATIO(.92),
        .high_moisture = MOISTURE_SENSOR_VALUE_FROM_RATIO(.93),
        .polling_period_s = 10,
        .pump_on_period_s = 1,
        .pump_off_period_s = 59,
        .wet_hold_period_s = 30*60,
        .dry_hold_period_s = 5*60
    },
    .status = plant_status_struct_default
};

// Global plant structure...  :(  Made it global so it can be modified by the mqtt thread.  Refactor this some day
struct plant_struct global_plant = plant_default;

esp_err_t store_plant_to_nvs(struct plant_struct *plant, const char *nvs_key);


// Processes JSON data received from mqtt in the following formats
/*
{
    "config":{ 
        "low_moisture":       0.80, 
        "watered_moisture":   0.92, 
        "high_moisture":      0.93, 
        "polling_period_s":     10, 
        "pump_on_period_s":      2, 
        "pump_off_period_s":    58, 
        "wet_hold_period_s":  1800, 
        "dry_hold_period_s":   300
    }
}
*/
void process_mqqt_data(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    cJSON *json = cJSON_Parse(event->data);
    
    if(0 == strncmp("query", event->data, sizeof("query")-1)){
        static char query_rsp[2048];
        sprintf(query_rsp, 
            "{\n"
            "     \"config\":{ \n"
            "         \"low_moisture\":       %0.2f, \n"
            "         \"watered_moisture\":   %0.2f, \n"
            "         \"high_moisture\":      %0.2f, \n"
            "         \"polling_period_s\":   %d, \n"
            "         \"pump_on_period_s\":   %d, \n"
            "         \"pump_off_period_s\":  %d, \n"
            "         \"wet_hold_period_s\":  %d, \n"
            "         \"dry_hold_period_s\":  %d \n"
            "    }\n"
            "}\n", 
            RATIO_FROM_MOISTURE_SENSOR_VALUE(global_plant.config.low_moisture), 
            RATIO_FROM_MOISTURE_SENSOR_VALUE(global_plant.config.watered_moisture), 
            RATIO_FROM_MOISTURE_SENSOR_VALUE(global_plant.config.high_moisture), 
            global_plant.config.polling_period_s, 
            global_plant.config.pump_on_period_s, 
            global_plant.config.pump_off_period_s, 
            global_plant.config.wet_hold_period_s, 
            global_plant.config.dry_hold_period_s );
        esp_mqtt_client_publish(client, "/topic/qos1", query_rsp, 0, 0, 0);
    }
    else if(json == NULL){
        ESP_LOGW(TAG, "Parse Error");
        esp_mqtt_client_publish(client, "/topic/qos1", "JSON PARSE ERROR", 0, 0, 0);
    }else{
        ESP_LOGI(TAG, "Parsed");
        cJSON *config = cJSON_GetObjectItemCaseSensitive(json, "config");
        
        // Parse config structure
        if(cJSON_IsObject(config)){
            cJSON *low_moisture_item = cJSON_GetObjectItemCaseSensitive(config, "low_moisture");
            cJSON *watered_moisture_item = cJSON_GetObjectItemCaseSensitive(config, "watered_moisture");
            cJSON *high_moisture_item = cJSON_GetObjectItemCaseSensitive(config, "high_moisture");
            cJSON *polling_period_s_item = cJSON_GetObjectItemCaseSensitive(config, "polling_period_s");
            cJSON *pump_on_period_s_item = cJSON_GetObjectItemCaseSensitive(config, "pump_on_period_s");
            cJSON *pump_off_period_s_item = cJSON_GetObjectItemCaseSensitive(config, "pump_off_period_s");
            cJSON *wet_hold_period_s_item = cJSON_GetObjectItemCaseSensitive(config, "wet_hold_period_s");
            cJSON *dry_hold_period_s_item = cJSON_GetObjectItemCaseSensitive(config, "dry_hold_period_s");
            // Validate all params exist and are correct type
            if(
                cJSON_IsNumber(low_moisture_item) &&
                cJSON_IsNumber(watered_moisture_item) &&
                cJSON_IsNumber(high_moisture_item) &&
                cJSON_IsNumber(polling_period_s_item) &&
                cJSON_IsNumber(pump_on_period_s_item) &&
                cJSON_IsNumber(pump_off_period_s_item) &&
                cJSON_IsNumber(wet_hold_period_s_item) &&
                cJSON_IsNumber(dry_hold_period_s_item))
            {
                // Simple sanity check of parameters
                if( low_moisture_item->valuedouble < watered_moisture_item->valuedouble && 
                    watered_moisture_item->valuedouble <= high_moisture_item->valuedouble && 
                    polling_period_s_item->valueint > 0 && 
                    pump_on_period_s_item->valueint > 0 && 
                    pump_off_period_s_item->valueint > 0 && 
                    wet_hold_period_s_item->valueint > 0 && 
                    dry_hold_period_s_item->valueint > 0)
                {
                    // Use these parameters and store in flash
                    global_plant.config.low_moisture = MOISTURE_SENSOR_VALUE_FROM_RATIO(low_moisture_item->valuedouble);
                    global_plant.config.watered_moisture = MOISTURE_SENSOR_VALUE_FROM_RATIO(watered_moisture_item->valuedouble);
                    global_plant.config.high_moisture = MOISTURE_SENSOR_VALUE_FROM_RATIO(high_moisture_item->valuedouble);
                    global_plant.config.polling_period_s = polling_period_s_item->valueint;
                    global_plant.config.pump_on_period_s = pump_on_period_s_item->valueint;;
                    global_plant.config.pump_off_period_s = pump_off_period_s_item->valueint;
                    global_plant.config.wet_hold_period_s = wet_hold_period_s_item->valueint;
                    global_plant.config.dry_hold_period_s = dry_hold_period_s_item->valueint;

                    ESP_ERROR_CHECK(store_plant_to_nvs(&global_plant, PLANT_NVS_KEY));
                    esp_mqtt_client_publish(client, "/topic/qos1", "CONFIG ACCEPTED", 0, 0, 0);
                }else{
                    esp_mqtt_client_publish(client, "/topic/qos1", "CONFIG REJECTED - Failed sanity check", 0, 0, 0);
                }
            }else{
                esp_mqtt_client_publish(client, "/topic/qos1", "CONFIG REJECTED - Parse Failed", 0, 0, 0);
            }
        }
        else
        {
            esp_mqtt_client_publish(client, "/topic/qos1", "Unexpected JSON structure", 0, 0, 0);
        }
    }

    cJSON_Delete(json);
}

static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            // TODO: Subscribe to topics here
            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            mqtt_connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected = false;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
                ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            process_mqqt_data(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

esp_mqtt_client_handle_t mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = CONFIG_MQTT_BROKER_URL,
        .username = CONFIG_MQTT_USERNAME,
        .password = CONFIG_MQTT_PASSWORD,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    return client;
}

void pollSensors(struct plant_struct* plant, uint64_t now, esp_mqtt_client_handle_t client)
{
    if(use_fake_poll){
        plant->status.poll_median_moisture_sensor = fake_moisture;
        plant->status.poll_median_level_sensor = fake_level;
        plant->status.last_poll_time_us = now;
    }
    else
    {
        int moisture_readings[9] = {0};
        int level_readings[9] = {0};

        for(int i = 0; i < 9; i++)
        {
            moisture_readings[i] = adc1_get_raw(plant->pins.moisture_sensor_adc1_channel);
            level_readings[i] = adc1_get_raw(plant->pins.level_sensor_adc1_channel);
        }

        plant->status.poll_median_moisture_sensor = opt_med9(moisture_readings);
        plant->status.poll_median_level_sensor = opt_med9(level_readings);
        plant->status.last_poll_time_us = now;

        dht_read_float_data(DHT_TYPE_DHT11, plant->pins.dht_gpio_pin, &(plant->status.poll_humidity), &(plant->status.poll_temperature));
    }

    size_t sum_heap_free = esp_get_free_heap_size();
    float moisture_percent = RATIO_FROM_MOISTURE_SENSOR_VALUE(plant->status.poll_median_moisture_sensor);

    if(client && mqtt_connected){
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "test", 100*moisture_percent);
        cJSON_AddNumberToObject(root, "temperature", plant->status.poll_temperature);
        cJSON_AddNumberToObject(root, "humidity", plant->status.poll_humidity);
        cJSON_AddNumberToObject(root, "water_available", plant->status.poll_median_level_sensor);
        cJSON_AddNumberToObject(root, "state", plant->status.state);
        cJSON_AddNumberToObject(root, "sum_heap_free", sum_heap_free);
        char *my_json_string = cJSON_Print(root);
        esp_mqtt_client_publish(client, "/test/test", my_json_string, 0, 0, 0);
        free(my_json_string); // Need to free the string allocated by cJSON_Print
        cJSON_Delete(root); // Free the cJSON object
    }

    ESP_LOGI(TAG, "[%s] moisture = %0.4f (%d), water_available = %d, temperature = %0.1f, humidity = %0.1f, state = %s, sum_heap_free=%d", 
        mqtt_connected?"connected":"DISCONNECTED", 
        moisture_percent, plant->status.poll_median_moisture_sensor, plant->status.poll_median_level_sensor, 
        plant->status.poll_temperature, plant->status.poll_humidity,
        PlantStateString[plant->status.state], sum_heap_free);
}

void turnOnPump(struct plant_struct* plant)
{
    if(enable_pump && plant->status.poll_median_level_sensor > 2048){
        gpio_set_level(plant->pins.pump_gpio_pin, 0); // Turn ON pump (active low)
    }
}

void turnOffPump(struct plant_struct* plant)
{
    gpio_set_level(plant->pins.pump_gpio_pin, 1); // Turn OFF pump (active low)
}

void initPlant(struct plant_struct* plant, uint64_t now, esp_mqtt_client_handle_t client)
{
    // Configure ADC1 level and moisture sensor channels
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(plant->pins.moisture_sensor_adc1_channel, ADC_ATTEN_MAX);   /*!< Moisture Sensor - ADC1 channel 4 is GPIO32 */
    adc1_config_channel_atten(plant->pins.level_sensor_adc1_channel, ADC_ATTEN_MAX);   /*!< Water Level Sensor - ADC1 channel 5 is GPIO33 */

    // Setup the GPIO pin for controlling the pump (pump is active low)
    gpio_pad_select_gpio(plant->pins.pump_gpio_pin);
    gpio_set_direction(plant->pins.pump_gpio_pin, GPIO_MODE_OUTPUT);
    turnOffPump(plant); // Turn pump OFF (active low)

    // Do initial sensor poll
    pollSensors(plant, now, client);

    plant->status.state_entry_time_us = now;
    plant->status.state = PLANT_DRYING;
    plant->status.initialized = true;
}

void changeState(struct plant_struct* plant, enum PlantStates new_state, uint64_t now){
    bool valid = false;

    switch(plant->status.state){
        case PLANT_DRYING:
            switch(new_state){
                case PLANT_DRY_HOLD: valid = true; break;
                default: /* invalid */ break;
            }
            break;
        case PLANT_DRY_HOLD:
            switch(new_state){
                case PLANT_PUMP_DELAY: valid = true; break;
                case PLANT_DRYING: valid = true; break;
                default: /* invalid */ break;
            }
            break;
        case PLANT_PUMP_DELAY:
            switch(new_state){
                case PLANT_PUMP_ON: valid = true; break;
                case PLANT_WET_HOLD: valid = true; break;
                default: /* invalid */ break;
            }
            break;
        case PLANT_PUMP_ON:
            switch(new_state){
                case PLANT_PUMP_DELAY: valid = true; break;
                default: /* invalid */ break;
            }
            break;
        case PLANT_WET_HOLD:
            switch(new_state){
                case PLANT_DRYING: valid = true; break;
                case PLANT_PUMP_DELAY: valid = true; break;
                default: /* invalid */ break;
            }
            break;
        default:
            break;
    }

    if(valid){
        if(new_state == PLANT_PUMP_ON)
        {
            turnOnPump(plant);
        }
        else
        {
            turnOffPump(plant);
        }
        ESP_LOGI(TAG, "%s -> %s %f", PlantStateString[plant->status.state], PlantStateString[new_state], ((float)now) / SEC_IN_MICROSEC);
        plant->status.state_entry_time_us = now;
        plant->status.state = new_state;
    }else{
        // Turn Pump Off
        ESP_LOGI(TAG, "ALARM!  %s -> %s %f", PlantStateString[plant->status.state], PlantStateString[new_state], ((float)now) / SEC_IN_MICROSEC);
        plant->status.state_entry_time_us = now;
        plant->status.state = PLANT_ALARM;
    }
}

void handleStateMachine(struct plant_struct* plant, esp_mqtt_client_handle_t client)
{
    uint64_t now = esp_timer_get_time();

    if(!plant->status.initialized)
    {
        initPlant(plant, now, client);
    }

    if(plant->status.state < PLANT_ALARM && now - plant->status.last_poll_time_us > plant->config.polling_period_s * SEC_IN_MICROSEC)
    {
        pollSensors(plant, now, client);
    }

    switch(plant->status.state){
        case PLANT_DRYING:
            if(plant->status.poll_median_moisture_sensor < plant->config.low_moisture)
            {
                changeState(plant, PLANT_DRY_HOLD, now);
            }
            else
            {
                // Stay in PLANT_DRYING state
            }
            break;
        case PLANT_DRY_HOLD:
            if(plant->status.poll_median_moisture_sensor > plant->config.low_moisture)
            {
                changeState(plant, PLANT_DRYING, now);
            }
            else if(now - plant->status.state_entry_time_us > plant->config.dry_hold_period_s * SEC_IN_MICROSEC)
            {
                changeState(plant, PLANT_PUMP_DELAY, now);
            }
            else
            {
                // Stay in PLANT_DRY_HOLD state
            }
            break;
        case PLANT_PUMP_DELAY:
            if(plant->status.poll_median_moisture_sensor >= plant->config.high_moisture)
            {
                changeState(plant, PLANT_WET_HOLD, now);
            }
            else if(now - plant->status.state_entry_time_us > plant->config.pump_off_period_s * SEC_IN_MICROSEC)
            {
                changeState(plant, PLANT_PUMP_ON, now);
            }
            else
            {
                // Stay in PLANT_PUMP_DELAY state
            }
            break;
        case PLANT_PUMP_ON:
            if(now - plant->status.state_entry_time_us > plant->config.pump_on_period_s * SEC_IN_MICROSEC)
            {
                changeState(plant, PLANT_PUMP_DELAY, now);
            }
            else
            {
                // Stay in PLANT_PUMP_ON state
            }
            break;
        case PLANT_WET_HOLD:
            if(plant->status.poll_median_moisture_sensor <= plant->config.watered_moisture)
            {
                changeState(plant, PLANT_PUMP_DELAY, now);
            }
            else if(now - plant->status.state_entry_time_us > plant->config.wet_hold_period_s * SEC_IN_MICROSEC)
            {
                changeState(plant, PLANT_DRYING, now);
            }
            else
            {
                // Stay in PLANT_WET_HOLD state
            }
            break;
        default:
            break;
    }
}

esp_err_t store_plant_to_nvs(struct plant_struct *plant, const char *nvs_key){
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Write value including previously saved blob if available
    err = nvs_set_blob(my_handle, nvs_key, plant, sizeof(*plant));

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;    
}

esp_err_t  read_plant_from_nvs(struct plant_struct *plant, const char *nvs_key){
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, nvs_key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Read previously saved blob if available
    if (required_size > 0) {
        err = nvs_get_blob(my_handle, nvs_key, plant, &required_size);
        // Reset stored status to default
        plant->status = plant_status_struct_default;
        if (err != ESP_OK) {
            return err;
        }else{
          ESP_LOGI(TAG, "Using stored data \"%s\"", nvs_key);
        }
    }else{
        ESP_LOGI(TAG, "Plant data for \"%s\" not found in NVS - Using default data", nvs_key);
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;    
}

void app_main(void)
{
    
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    // Initialize flash if not already initialized
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(read_plant_from_nvs(&global_plant, PLANT_NVS_KEY));
    print_plant_struct(&global_plant);

    // Enable wifi and mqtt by removing these comments
    wifi_init_sta();
    esp_mqtt_client_handle_t client = mqtt_app_start();

    while(1)
    {
        handleStateMachine(&global_plant, client);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Allow other tasks to run
    }
}
