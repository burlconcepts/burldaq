/*
 * Copyright (C) 2024 BURL Concepts, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef BURLDAQ_H
#define BURLDAQ_H
#ifdef __cplusplus
extern "C" { 
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <asm/types.h>
#include <ctype.h>
#include <stdbool.h>
#include <dirent.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>

/* function prototypes for burl native DAQ */
int DAQ_Init();
int DAQ_StartCapture();
void DAQ_Terminate();
int DAQ_StopCapture();
bool DAQ_IsDeviceConnected();
int DAQ_EnableDebugMode(bool enable);
int DAQ_SetResultFileName(const char* filename);
int DAQ_IsScanReady();
int DAQ_IsDAQResponding();
int DAQ_ScanFailures();
int DAQ_ConfigureSpiFrequency(int freq);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif // BURLDAQ_H
