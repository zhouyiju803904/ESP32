#line 1 "C:\\Users\\user\\Desktop\\ESP32\\src\\i2c\\i2c.cpp"
/*****************************************************************************
 * | File         :   i2c.c
 * | Author       :   Waveshare team
 * | Function     :   Hardware underlying interface
 * | Info         :
 * |                 I2C driver code for I2C communication.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-26
 * | Info         :   Basic version
 *
 ******************************************************************************/

#include "i2c.h"  // Include I2C driver header for I2C functions

// Global handle for the I2C master bus
// i2c_master_bus_handle_t bus_handle = NULL;
DEV_I2C_Port handle;
/**
 * @brief Initialize the I2C master interface.
 * 
 * This function configures the I2C master bus and adds a device with the specified address.
 * The I2C clock source, frequency, SCL/SDA pins, and other settings are configured here.
 * 
 * @param Addr The I2C address of the device to be initialized.
 * @return The device handle if initialization is successful, NULL otherwise.
 */
DEV_I2C_Port DEV_I2C_Init()
{
    Wire.begin(EXAMPLE_I2C_MASTER_SDA, EXAMPLE_I2C_MASTER_SCL);
    Wire.setClock(EXAMPLE_I2C_MASTER_FREQUENCY);
    handle.bus = EXAMPLE_I2C_MASTER_NUM;
    handle.dev = 0;
    return handle;  // Return the device handle if successful
}

/**
 * @brief Set a new I2C slave address for the device.
 * 
 * This function allows changing the I2C slave address for the specified device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Addr The new I2C address for the device.
 */
void DEV_I2C_Set_Slave_Addr(i2c_master_dev_handle_t *dev_handle, uint8_t Addr)
{
    *dev_handle = Addr;
}

/**
 * @brief Write a single byte to the I2C device.
 * 
 * This function sends a command byte and a value byte to the I2C device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param value The value byte to send.
 */
void DEV_I2C_Write_Byte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t value)
{
    Wire.beginTransmission(dev_handle);
    Wire.write(Cmd);
    Wire.write(value);
    Wire.endTransmission();
}

/**
 * @brief Read a single byte from the I2C device.
 * 
 * This function reads a byte of data from the I2C device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @return The byte read from the device.
 */
uint8_t DEV_I2C_Read_Byte(i2c_master_dev_handle_t dev_handle)
{
    Wire.requestFrom(dev_handle, static_cast<uint8_t>(1));
    return Wire.available() ? Wire.read() : 0;
}

/**
 * @brief Read a word (2 bytes) from the I2C device.
 * 
 * This function reads two bytes (a word) from the I2C device.
 * The data is received by sending a command byte and receiving the data.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @return The word read from the device (combined two bytes).
 */
uint16_t DEV_I2C_Read_Word(i2c_master_dev_handle_t dev_handle, uint8_t Cmd)
{
    uint8_t data[2] = {0, 0};
    DEV_I2C_Read_Nbyte(dev_handle, Cmd, data, 2);
    return data[1] << 8 | data[0];
}

/**
 * @brief Write multiple bytes to the I2C device.
 * 
 * This function sends a block of data to the I2C device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param pdata Pointer to the data to send.
 * @param len The number of bytes to send.
 */
void DEV_I2C_Write_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t *pdata, uint8_t len)
{
    Wire.beginTransmission(dev_handle);
    Wire.write(pdata, len);
    Wire.endTransmission();
}

/**
 * @brief Read multiple bytes from the I2C device.
 * 
 * This function reads multiple bytes from the I2C device.
 * The function sends a command byte and receives the specified number of bytes.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param pdata Pointer to the buffer where received data will be stored.
 * @param len The number of bytes to read.
 */
void DEV_I2C_Read_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t *pdata, uint8_t len)
{
    Wire.beginTransmission(dev_handle);
    Wire.write(Cmd);
    Wire.endTransmission(false);
    Wire.requestFrom(dev_handle, len);

    for (uint8_t index = 0; index < len; ++index)
    {
        pdata[index] = Wire.available() ? Wire.read() : 0;
    }
}
