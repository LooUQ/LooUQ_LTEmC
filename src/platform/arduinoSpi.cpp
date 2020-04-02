// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoSpi.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include "platformSpi.h"
//#include <Arduino.h>
//#include <Wire.h>
#include <SPI.h>

SPISettings nxpSettings;

spi_device spi_init(spi_config_t config)
{
    //spi_settings = new SPISettings(config.dataRate, (BitOrder)config.bitOrder, config.dataMode);

    spi_device spi = (spi_device_t*)malloc(sizeof(spi_device_t));
	if (spi == NULL)
	{
		return NULL;
	}	

    spi->config = (spi_config_t*)malloc(sizeof(spi_config_t));
	if (spi->config == NULL)
	{
        free(spi);
		return NULL;
	}

    spi->config->dataRate = config.dataRate;
    spi->config->dataMode = config.dataMode;
    spi->config->bitOrder = config.bitOrder;
    spi->config->csPin = config.csPin;

    nxpSettings = SPISettings(spi->config->dataRate, (BitOrder)spi->config->bitOrder, spi->config->dataMode);

    digitalWrite(spi->config->csPin, HIGH);
    pinMode(spi->config->csPin, OUTPUT);
    SPI.begin();

    return spi;
}


void spi_uninit(spi_device spi)
{
    SPI.end();
    free(spi->config);
    free(spi);
}


uint8_t spi_transferByte(spi_device spi, uint8_t writeVal)
{
    digitalWrite(spi->config->csPin, LOW);
    SPI.beginTransaction(nxpSettings);

    uint8_t readVal = SPI.transfer(writeVal);

    digitalWrite(spi->config->csPin, HIGH);
    SPI.endTransaction();
    return readVal;
}


uint16_t spi_transferWord(spi_device spi, uint16_t writeVal)
{
    digitalWrite(spi->config->csPin, LOW);
    SPI.beginTransaction(nxpSettings);

    uint16_t readVal = SPI.transfer16(writeVal);
    
    digitalWrite(spi->config->csPin, HIGH);
    SPI.endTransaction();
    return readVal;
}


int spi_transferBuffer(spi_device spi, const void* src, void* dest, size_t xfer_len)
{
    digitalWrite(spi->config->csPin, LOW);
    SPI.beginTransaction(nxpSettings);

    SPI.transfer(src, dest, xfer_len);

    digitalWrite(spi->config->csPin, HIGH);
    SPI.endTransaction();
}

