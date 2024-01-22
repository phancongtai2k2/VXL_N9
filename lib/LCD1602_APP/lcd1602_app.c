#include "lcd1602_app.h"
#include "esp_log.h"
smbus_info_t * smbus_info;
int i2c_master_port = I2C_MASTER_NUM;
i2c_config_t conf;
i2c_port_t i2c_num = I2C_MASTER_NUM;
uint8_t address = LCD1602_I2C_ADDRESS;


esp_err_t lcd1602_init(i2c_lcd1602_info_t * lcd_info){
     // Set up I2C
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;  // GY-2561 provides 10kΩ pullups
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;  // GY-2561 provides 10kΩ pullups
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_LEN,
                       I2C_MASTER_TX_BUF_LEN, 0);

    // Set up the SMBus
    smbus_info = smbus_malloc();
    ESP_ERROR_CHECK(smbus_init(smbus_info, i2c_num, address));
    ESP_ERROR_CHECK(smbus_set_timeout(smbus_info, 1000 / portTICK_PERIOD_MS));

    // Set up the LCD1602 device with backlight off
    // lcd_info = i2c_lcd1602_malloc();
    ESP_ERROR_CHECK(i2c_lcd1602_init(lcd_info, smbus_info, true,LCD_NUM_ROWS, LCD_NUM_COLUMNS, LCD_NUM_VISIBLE_COLUMNS));

    ESP_ERROR_CHECK(i2c_lcd1602_reset(lcd_info));
    //turn off backlight
    i2c_lcd1602_set_backlight(lcd_info, false);

    //turn on backlight
    i2c_lcd1602_set_backlight(lcd_info, true);
    i2c_lcd1602_set_cursor(lcd_info, true);
    i2c_lcd1602_set_cursor(lcd_info, false);

    return ESP_OK;
}

esp_err_t lcd1602_updateScreen(i2c_lcd1602_info_t * lcd_info,dht11_reading *dht11, MQ6 *mq6 ){
    char tempConvertedToString[6];
    char humConvertedToString[6];
    char ppmConvertedToString[10];

    i2c_lcd1602_move_cursor(lcd_info, 0, 0);
    i2c_lcd1602_write_string(lcd_info,"RH:");
    sprintf(humConvertedToString,"%d",dht11->humidity);
    i2c_lcd1602_write_string(lcd_info,humConvertedToString);
    i2c_lcd1602_write_char(lcd_info,'%');

    i2c_lcd1602_move_cursor(lcd_info,8,0);
    sprintf(tempConvertedToString,"%d",dht11->temperature);
    i2c_lcd1602_write_string(lcd_info,tempConvertedToString);
    i2c_lcd1602_write_string(lcd_info,"oC");

    i2c_lcd1602_move_cursor(lcd_info,0,1);
    i2c_lcd1602_write_string(lcd_info,"PPM GAS:");
    i2c_lcd1602_move_cursor(lcd_info,10,1);
    sprintf(ppmConvertedToString,"%d",mq6->ppm);
    i2c_lcd1602_write_string(lcd_info,ppmConvertedToString);
    i2c_lcd1602_write_string(lcd_info,"       ");

    ESP_LOGI(__func__, "Updated LCD1602.");

    return ESP_OK;
}

   