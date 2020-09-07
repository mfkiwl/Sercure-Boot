/**
  ******************************************************************************
  * @file    mapping_fwimg.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for firmware images mapping
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright(c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef MAPPING_FWIMG_H
#define MAPPING_FWIMG_H

/* Slots Regions must be aligned on 8192 bytes (0x2000) */

/* swap region (16 kbytes) */
#define REGION_SWAP_START                   0x08104000
#define REGION_SWAP_END                     0x08107FFF

/* slot 0 region (976 kbytes) */
#define REGION_SLOT_0_START                 0x08010000
#define BANK1_SECURE_END                    0x080101FF /* Secure User Memory: end of the protected area */      
#define REGION_SLOT_0_END                   0x08103FFF

/* slot 1 region (976 kbytes) */
#define REGION_SLOT_1_START                 0x08108000
#define REGION_SLOT_1_END                   0x081FBFFF

#endif
