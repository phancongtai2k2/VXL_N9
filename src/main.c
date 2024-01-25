#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "dht11.h"
#include "wifiesp.h"
#include "MQ6.h"
#include "lcd1602_app.h"

#include "Rx.h"

/* --------------------- Define -------------------- */

#define TAG "RF433"

#define BUZZER_PIN 2
#define DHT11_PIN 4  
#define LED_WIFI_PIN 12

#define WEB_SERVER "api.thingspeak.com"
#define WEB_PORT "80"
#define THINGSPEAK_API_KEY "F8AGJM5KOU6H4RNM"

/* --------------------- Variable -------------------- */
static const char *REQUEST_TEMPLATE = "GET /update?api_key=%s&field1=%d&field2=%d&field3=%d&field4=%f HTTP/1.0\r\n"
                                      "Host: "WEB_SERVER"\r\n"
                                      "User-Agent: esp-idf/1.0 esp32\r\n"
                                      "\r\n";
static EventGroupHandle_t s_wifi_event_group;
gpio_config_t io_conf;
i2c_lcd1602_info_t * lcd_info;
dht11_reading dht11;
MQ6 mq6;
float prob_DS;
/* --------------------- Function -------------------- */
//Function to connect ThinkSpeak
static void send_to_thingspeak(int temparature, int humidity, int ppm, float prob_DS)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s;
    char request[256];

    int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
    if(err != 0 || res == NULL) {
        ESP_LOGE(__func__, "DNS lookup failed err=%d res=%p", err, res);
        return;
    }

    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(__func__, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0) {
        ESP_LOGE(__func__, "Failed to allocate socket.");
        freeaddrinfo(res);
        return;
    }

    if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(__func__, "Socket connect failed errno=%d", errno);
        close(s);
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);

    snprintf(request, sizeof(request), REQUEST_TEMPLATE, THINGSPEAK_API_KEY, temparature, humidity, ppm, prob_DS);

    if (write(s, request, strlen(request)) < 0) {
        ESP_LOGE(__func__, "Socket send failed");
        close(s);
        return;
    }

    ESP_LOGI(__func__, "Data sent to Thingspeak");

    close(s);
}

//Function handle event with wifi 
static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    ESP_LOGI(__func__,"Start wifi_event_handler");
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(__func__,"Turn off led");
        gpio_set_level(LED_WIFI_PIN,0);
        esp_wifi_connect();
        ESP_LOGI(__func__, "retry to connect to the AP");
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(__func__,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(__func__, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(__func__,"Turn on led");
        gpio_set_level(LED_WIFI_PIN,1);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } 
}

//Function to init wifi sta 
void wifi_init_sta()
{   

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    // if(checkNetif==false){
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
        // checkNetif=true;
    // }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(__func__, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(__func__, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
     
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(__func__, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(__func__, "UNEXPECTED EVENT");
    }
}

void checkFire_DS()
{
	if(prob_DS>0.5)
	{
        gpio_set_level(BUZZER_PIN, 1); 
        ESP_LOGI(__func__, "Detecting Fire !!!!!!! with  %f\n",prob_DS);
	}
	else{
        gpio_set_level(BUZZER_PIN, 0); 
        ESP_LOGI(__func__, "No Fire! with prob_DS: %f\n",prob_DS);
    }
}

void readDataFromSensor()
{   
        printf("Read DHT11.\n");
        dht11 = DHT11_read();

        if (dht11.humidity < 0 || dht11.temperature < 0)
        {
            dht11.humidity = 0;
            dht11.temperature = 0;
        }

        printf("Read MQ6.\n");
        mq6 = MQ6_readData();
        if(mq6.ppm >= 10000){
            mq6.ppm = 10000;
        }
        ESP_LOGI(__func__,"Do am: %d, Nhiet do: %d, PPM: %d\n",dht11.humidity,dht11.temperature,mq6.ppm);

        // Tính Toán độ chính xác của MQ6 dựa vào nhiệt độ //
        double y_percent_temp ;
        y_percent_temp = (((double)1/480) * pow( (double) dht11.temperature,3.0) - ((double)5/16) * pow((double)dht11.temperature,2.0) + ((double)125/12) * (double) dht11.temperature - 0.234)/100 ;
        printf("------PHAN TRAM NHIET DO------- %lf \n\n" ,y_percent_temp);

        // Tính Toán độ chính xác của MQ6 dựa vào độ ẩm //
        double y_percent_hum ;
        y_percent_hum = (((double)-4/5175) * pow((double)dht11.humidity,3.0) + ((double)16/345) * pow((double)dht11.humidity,2.0) + ((double)592/207) * (double)dht11.humidity - ((double)1640/23) - 0.306)/100 ;
        printf("-----PHAN TRAM ĐỘ ẨM-------- %lf \n\n" ,y_percent_hum);

        // Sử dụng DS tính độ tin cậy của cảm biến MQ6 //

        double y_percent_mq ;
        y_percent_mq= (y_percent_hum * y_percent_temp)/(1-(((1-y_percent_hum)*y_percent_temp)+(1-y_percent_temp)*y_percent_hum));
        printf("-----PHAN TRAM MQ-------- %lf \n\n" ,y_percent_mq);
       
     


        // DS cho đa cảm biến //

        float t = (float)dht11.temperature/50;
	    float h = 1-((float)dht11.humidity-20)/70;

        float prob_DSth = (t*h + 0.08*h + 0.15*t) / (1-((1-h-0.15)*t + (1-t-0.08)*h));

        // Độ trị tin cậy MQ6 //
        double ppm0 =  y_percent_mq * mq6.ppm ;

        printf("--------------------------- %f \n\n" ,y_percent_mq);
        printf("--------------------------- %lf \n\n" ,ppm0);

        float ppm1 = (ppm0-300)/(10000-300);

        prob_DS = (ppm1*prob_DSth + 0.05*prob_DSth + 0.012*ppm1) / (1-((1-prob_DSth-0.012)*ppm1 + (1-ppm1-0.05)*prob_DSth));

        checkFire_DS();
}

void app_main(void)
{
    ESP_LOGI(__func__,"------------------------ Main Function ------------------------");
    vTaskDelay(200/portTICK_PERIOD_MS);
    /* Initialize NVS */
    ESP_ERROR_CHECK(nvs_flash_init());
    /* Initialize Buzzer and Led */
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = (1ULL << DHT11_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << BUZZER_PIN),
    io_conf.mode = GPIO_MODE_OUTPUT,
    io_conf.intr_type = GPIO_INTR_DISABLE,
    io_conf.pull_down_en = 0,
    io_conf.pull_up_en = 0,
    gpio_config(&io_conf);
    gpio_set_level(BUZZER_PIN, 0); 

    io_conf.pin_bit_mask = (1ULL << LED_WIFI_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE; // Vô hiệu pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Vô hiệu pull-down
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(LED_WIFI_PIN, 0); 

    /* Initialize Wifi */ 
    wifi_init_sta();

    /* Initialize DHT11 */
    DHT11_init(DHT11_PIN);

    /* Initialize MQ6 */
    MQ6_init();   

    /* Initialize LCD1602 */
    lcd_info = i2c_lcd1602_malloc();
    lcd1602_init(lcd_info);
    
    while(1){
        ESP_LOGI(__func__,"/*----------------------- WHILE LOOP BEGIN --------------------*/");
        //time start 
        uint64_t start_time = esp_timer_get_time();
        
        readDataFromSensor();

        lcd1602_updateScreen(lcd_info,&dht11,&mq6);

        vTaskDelay(2500/portTICK_PERIOD_MS);  // Wifi check here

        printf("Connect to ThinkSpeak\n");
        send_to_thingspeak(dht11.temperature,dht11.humidity,mq6.ppm,prob_DS);

        //get time end
        uint64_t end_time = esp_timer_get_time();
        uint64_t execution_time = end_time - start_time;
        printf("Elapsed time : %llu miliseconds\n", execution_time/1000);    
        ESP_LOGI(__func__,"/* ----------------------- WHILE LOOP END --------------------*/\n");   
    }
}



