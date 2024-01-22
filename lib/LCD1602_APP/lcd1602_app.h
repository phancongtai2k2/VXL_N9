#ifndef _LCD1602_APP_H_
#define _LCD1602_APP_H_

#include "i2c-lcd1602.h"
#include "MQ6.h"
#include "DHT11.h"

// LCD1602
#define LCD_NUM_ROWS               2
#define LCD_NUM_COLUMNS            32
#define LCD_NUM_VISIBLE_COLUMNS    16

#define I2C_MASTER_NUM           I2C_NUM_0
#define I2C_MASTER_TX_BUF_LEN    0                     // disabled
#define I2C_MASTER_RX_BUF_LEN    0                     // disabled
#define I2C_MASTER_FREQ_HZ       100000
#define I2C_MASTER_SDA_IO        21
#define I2C_MASTER_SCL_IO        22
#define LCD1602_I2C_ADDRESS      0x27



/**
 * @brief Initialize I2C and driver for LCD1602 display
 * @return esp_err_t
*/
esp_err_t lcd1602_init(i2c_lcd1602_info_t * lcd_info);


/**
 * @brief Update data and time to screen
 * @param data_sensor
 * @return esp_err_t
*/
esp_err_t lcd1602_updateScreen(i2c_lcd1602_info_t * lcd_info,dht11_reading *dht11, MQ6 *mq6 );


#endif //_LCD1602_APP_H_