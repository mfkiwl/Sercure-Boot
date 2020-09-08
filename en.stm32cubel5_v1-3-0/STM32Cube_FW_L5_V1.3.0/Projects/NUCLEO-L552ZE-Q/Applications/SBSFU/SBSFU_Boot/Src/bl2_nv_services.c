/**
  ******************************************************************************
  * @file    bl2_nv_service.c
  * @author  MCD Application Team
  * @brief   This file provides all the Non volatile firmware functions.
  @verbatim
  ==============================================================================

                   ##### How to use this driver #####
  ==============================================================================
    [..]
      This driver provides functions to initialize Non Volatile Storage, to read and
      write Non volatile Monothonic Counters.

      (#) NVCNT initialization functions:
               (++) Initialize NVCNT using tfm_plat_init_nv_counter fucntion,
         it must be performed at system start up.
         At first boot, on a virgin flash, the NV area is formatted and
         HUK seed (32bits) is randomly generated and written in NV area.

      (#) NVCNT counter access functions:
           (++) Write Monothonic counter using tfm_plat_set_nv_counter or tfm_plat_set_nv_counter functions
                A Clean Up request can be raised as return parameter in case
                FLASH pages used by EEPROM emulation, are full.
           (++) Read monothonic counter using tfm_plat_read_nv_counter functions

  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "boot_hal_cfg.h"
#include "string.h"
#include "platform/include/tfm_plat_nv_counters.h"
#ifdef TFM_DEV_MODE
#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#else
#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_OFF
#endif /* TFM_DEV_MODE */
#include "bootutil/bootutil_log.h"
#include "tfm_boot_status.h"
#include "boot_record.h"
#include <limits.h>
#include "Driver_Flash.h"
#include "flash_layout.h"
#include "stm32l5xx_ll_crc.h"
#include "stm32l5xx_ll_bus.h"

/** @defgroup NVCNT Implementation NVCNT Implementation
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/** @defgroup NVCNT_Private_Structures NVCNT Private Structures
  * @{
  */


/**
  * @brief  NVCNT State Type structure definition.
  */

/**
  * @}
  */

/* Private variables ---------------------------------------------------------*/
/** @defgroup NVCNT_Private_Variables NVCNT Private Variables
  * @{
  */
extern ARM_DRIVER_FLASH FLASH_DEV_NAME;
/*  HUK is 32 bytes , each element of 8 bytes contains 4 bytes of data */
#define HUK_SIZE 32U
#define NVCNT_ID_HUK ((NVCNT_ID_TYPE)0x4855)
#define PAGE_HEADER_SIZE  (2*HUK_SIZE)
#define NVCNT_HEADER_VALUE (uint32_t)(0xaaddeecc)
/* Global variables used to store eeprom status */
static uint32_t uhNbWrittenElements = 0U;                  /*!< Nb of elements written in valid and active pages */
static uint32_t uwAddressNextWrite = PAGE_HEADER_SIZE;     /*!< Initialize write position just after page header */

/**
  * @}
  */
/* Private configuration  -----------------------------------------------*/
/** @defgroup NVCNT_Private_configuration NVCNT Private configuration
  * @{
  */
/* Configuration of crc calculation for eeprom emulation in flash */
#define NVCNT_ID_TYPE  uint16_t                      /*!< Type of Virtual Address */
#define NVCNT_ID_SHIFT 0U                            /*!< Bits Shifting to get Virtual Address in element */
#define NVCNT_CRC_TYPE             uint16_t                      /*!< Type of Crc */
#define NVCNT_CRC_SHIFT            16U                           /*!< Bits Shifting to get Crc in element */
#define NVCNT_DATA_TYPE            uint32_t                      /*!< Type of Data */
#define NVCNT_DATA_SHIFT           32U                           /*!< Bits Shifting to get Data value in element */
#define NVCNT_MASK_ID              (uint64_t)0x000000000000FFFFU
#define NVCNT_MASK_CRC             (uint64_t)0x00000000FFFF0000U
#define NVCNT_MASK_DATA            (uint64_t)0xFFFFFFFF00000000U
#define NVCNT_MASK_FULL            (uint64_t)0xFFFFFFFFFFFFFFFFU
/*!< Get Crc value from element value */
#define NVCNT_CRC_VALUE(__ELEMENT__) (NVCNT_CRC_TYPE)(((__ELEMENT__) & NVCNT_MASK_CRC) >> NVCNT_CRC_SHIFT)
/*!< Get element value from id, data and crc values */
#define NVCNT_ELEMENT_VALUE(__ID__,__DATA__,__CRC__) (((NVCNT_ELEMENT_TYPE)(__DATA__) << NVCNT_DATA_SHIFT)\
                                                      | (__CRC__) << NVCNT_CRC_SHIFT | (__ID__))
#define CRC_POLYNOMIAL_LENGTH   LL_CRC_POLYLENGTH_16B /* CRC polynomial lenght 16 bits */
#define CRC_POLYNOMIAL_VALUE    0x8005U /* Polynomial to use for CRC calculation */
#define NVCNT_ELEMENT_TYPE uint64_t
#define NVCNT_ELEMENT_SIZE sizeof(NVCNT_ELEMENT_TYPE)
/*!< Flash value after erase */
#define NVCNT_PAGESTAT_ERASED (uint64_t)0xFFFFFFFFFFFFFFFFU /*!< Flash value after erase */
#define NVCNT_MAX_WRITTEN_ELEMENTS ((BL2_NV_COUNTERS_AREA_SIZE - PAGE_HEADER_SIZE) / NVCNT_ELEMENT_SIZE)
/*!< Get id value from element value */
#define NVCNT_ID_VALUE(__ELEMENT__) (NVCNT_ID_TYPE)((__ELEMENT__) & NVCNT_MASK_ID)
/*!< Get Data value from element value*/
#define NVCNT_DATA_VALUE(__ELEMENT__) (NVCNT_DATA_TYPE)(((__ELEMENT__)\
                                                         & NVCNT_MASK_DATA) >> NVCNT_DATA_SHIFT)

#define NVCNT_INIT_VALUE 0U
/* Private function prototypes -----------------------------------------------*/
/** @defgroup NCNT_Private_Functions EEPROM Private Functions
  * @{
  */

static bool VerifyPageFullyErased(uint32_t Address, uint32_t PageSize);
static void ConfigureCrc(void);
static uint16_t CalculateCrc(NVCNT_DATA_TYPE Data, uint16_t CounterIc);
static bool Check_Header(void);
static bool Write_Header(void);
static bool init_counter(void);
/**
  * @}
  */
/* Exported functions -------------------------------------------------------*/
/** @addtogroup EEPROM_Exported_Functions
  * @{
  */

/**
  * @brief  Initialize nv counter storage block if not formatted.
  *         Check consistency of nv counter storage
  *         Initialize global variable for write access.
  *
  * @retval enum tfm_plat_err_t
  *           - TFM_PLAT_ERR_SUCCESS in case of success
  *           - TFM_PLAT_ERR_SYSTEM_ERR in case of error
  */
enum tfm_plat_err_t tfm_plat_init_nv_counter(void)
{

  int32_t  err;
  uint32_t  varidx = 0U;
  NVCNT_ELEMENT_TYPE addressvalue = 0U;
  NVCNT_DATA_TYPE counter_value;

  /*********************************************************************/
  /* Step 0: test if it is a full erased page                          */
  /*********************************************************************/
  if (VerifyPageFullyErased(BL2_NV_COUNTERS_AREA_ADDR, BL2_NV_COUNTERS_AREA_SIZE))
  {
    /* page is fully erase */
    /* no power down/reset is expected during this 1st initialization  */
    /* a regression is required to be able to restart the initialization */
    /*  uncomplete initialization is detected in step 1, and bl2 will */
    /*  not launch installed image*/

    /* Delay to avoid voltage rebounce effect (power down) at start up
       during BL2 NV area initialization period */
    HAL_Delay(500);

    BOOT_LOG_INF("Initializing BL2 NV area : Power down/reset not supported...");
    if (!Write_Header())
    {
      BOOT_LOG_ERR("Init BL2 NV Header area: Failed");
      return TFM_PLAT_ERR_SYSTEM_ERR;
    }
    BOOT_LOG_INF("Init BL2 NV Header area: Done");
    /* initialize nv counter 3 to 0 */
    BOOT_LOG_INF("Initializing BL2 NV Counters");

    if (!init_counter())
    {
      BOOT_LOG_ERR("Init BL2 NV counters : Failed");
      return TFM_PLAT_ERR_SYSTEM_ERR;
    }
    BOOT_LOG_INF("Init BL2 NV counters to 0 : Done");
    BOOT_LOG_INF("BL2 NV Area Initialized : Power Down/reset supported");
  }
  /*********************************************************************/
  /* Step 1: verify header presence                                    */
  /*                                                                   */
  /*********************************************************************/
  BOOT_LOG_INF("Checking BL2 NV area");
  BOOT_LOG_INF("Checking BL2 NV area header");
  if (!Check_Header())
  {
    BOOT_LOG_ERR("Wrong BL2 NV Area header");
    return TFM_PLAT_ERR_SYSTEM_ERR;
  }
  BOOT_LOG_INF("Checking BL2 NV Counter consitency");
  if (tfm_plat_read_nv_counter(PLAT_NV_COUNTER_3, sizeof(counter_value),
                               (uint8_t *)&counter_value) != TFM_PLAT_ERR_SUCCESS)
  {
    BOOT_LOG_ERR("NV Counter Not consitent %d", PLAT_NV_COUNTER_3);
    return TFM_PLAT_ERR_SYSTEM_ERR;
  }
  BOOT_LOG_INF("Consistent BL2 NV Counter %d  = 0x%x", PLAT_NV_COUNTER_3, counter_value);
  if (tfm_plat_read_nv_counter(PLAT_NV_COUNTER_4, sizeof(counter_value),
                               (uint8_t *)&counter_value) != TFM_PLAT_ERR_SUCCESS)
  {
    BOOT_LOG_ERR("NV Counter Not consitent %d", PLAT_NV_COUNTER_4);
    return TFM_PLAT_ERR_SYSTEM_ERR;
  }
  BOOT_LOG_INF("Consistent BL2 NV Counter %d  = 0x%x", PLAT_NV_COUNTER_4, counter_value);

  /*********************************************************************/
  /* Step 2: Initialize eeprom emulation global variables relative     */
  /*                                                                   */
  /*********************************************************************/
  /* Initialize global variables, with elements detected in active page */
  uhNbWrittenElements = 0U;
  uwAddressNextWrite = PAGE_HEADER_SIZE;

  for (varidx = PAGE_HEADER_SIZE; varidx < BL2_NV_COUNTERS_AREA_SIZE; varidx += NVCNT_ELEMENT_SIZE)
  {
    /* Check elements present in active page */
    err = FLASH_DEV_NAME.ReadData(BL2_NV_COUNTERS_AREA_ADDR + varidx, &addressvalue,
                                  sizeof(addressvalue));
    if ((err == ARM_DRIVER_ERROR_SPECIFIC) || (addressvalue != NVCNT_MASK_FULL))
    {
      /* Then increment uhNbWrittenElements and uwAddressNextWrite */
      uhNbWrittenElements++;
      uwAddressNextWrite += NVCNT_ELEMENT_SIZE;
    }
    else /* no more element in the page */
    {
      if (err != ARM_DRIVER_OK)
      {
        return TFM_PLAT_ERR_SYSTEM_ERR;
      }
      break;
    }
  }

  /*********************************************************************/
  /* Step 2 : Check consistancy of PLAT_NV_COUNTER_3 counter history   */
  /*********************************************************************/
  return TFM_PLAT_ERR_SUCCESS;
}

/**
  * @brief  Writes/updates counter data in NV area.
  * @warning This function is not reentrant
  * @param  CounterId to be written
  * @param  Data 32bits data to be written
  * @retval enum tfm_plat_err_t
  *           TFM_PLAT_ERR_SUCCESS on write sucessfully done.
              TFM_PLAT_ERR_MAX_VALUE when max updateable value written
  */
enum tfm_plat_err_t tfm_plat_set_nv_counter(enum tfm_nv_counter_t CounterId,
                                            uint32_t Data)
{
  int32_t err;
  uint32_t crc = 0U;
  NVCNT_ELEMENT_TYPE element;
  NVCNT_DATA_TYPE current_value;
  /* check current value is not already in flash, and is consistent */
  if ((tfm_plat_read_nv_counter(CounterId, sizeof(current_value), (uint8_t *)&current_value)
       != TFM_PLAT_ERR_SUCCESS) || (Data < current_value))
  {
    return TFM_PLAT_ERR_SYSTEM_ERR;
  }
  /* check if same value is requested */
  if (current_value == Data)
  {
    /* nothing to do */
    return TFM_PLAT_ERR_SUCCESS;
  }

  /* Check if pages are full, i.e. max number of written elements achieved */
  if (uhNbWrittenElements >= NVCNT_MAX_WRITTEN_ELEMENTS)
  {
    return TFM_PLAT_ERR_MAX_VALUE;
  }

  /* Calculate crc of variable data and virtual address */
  crc = CalculateCrc(Data, CounterId);
  /*  build element  */
  element = NVCNT_ELEMENT_VALUE(CounterId, Data, crc);
  /* Program variable data + virtual address + crc */
  err = FLASH_DEV_NAME.ProgramData(BL2_NV_COUNTERS_AREA_ADDR + uwAddressNextWrite,
                                   &element,
                                   NVCNT_ELEMENT_SIZE);
  /* If program operation was failed, a Flash error code is returned */
  if (err != ARM_DRIVER_OK)
  {
    return TFM_PLAT_ERR_SYSTEM_ERR;
  }

  /* Increment global variables relative to write operation done*/
  uwAddressNextWrite += NVCNT_ELEMENT_SIZE;
  uhNbWrittenElements++;
  BOOT_LOG_INF("Counter %d set to 0x%x", CounterId, Data);
  return TFM_PLAT_ERR_SUCCESS;
}

/**
  * @brief  Increment/updates counter data in NV area.
  * @warning This function is not reentrant
  * @param  CounterId to be written
   * @retval enum tfm_plat_err_t
  *           TFM_PLAT_ERR_SUCCESS on write sucessfully done.
              TFM_PLAT_ERR_MAX_VALUE when max updateable value written
  */

enum tfm_plat_err_t tfm_plat_increment_nv_counter(
  enum tfm_nv_counter_t counter_id)
{
  uint32_t security_cnt;
  enum tfm_plat_err_t err;

  err = tfm_plat_read_nv_counter(counter_id,
                                 sizeof(security_cnt),
                                 (uint8_t *)&security_cnt);
  if (err != TFM_PLAT_ERR_SUCCESS)
  {
    return err;
  }

  if (security_cnt == UINT32_MAX)
  {
    return TFM_PLAT_ERR_MAX_VALUE;
  }

  return tfm_plat_set_nv_counter(counter_id, security_cnt + 1u);
}

enum tfm_plat_err_t tfm_plat_read_nv_counter(enum tfm_nv_counter_t counter_id,
                                             uint32_t size, uint8_t *val)
{
  NVCNT_ELEMENT_TYPE addressvalue = 0U;
  uint32_t counter = 0U;
  uint32_t crc = 0U;
  uint32_t found = 0;
  uint32_t previous_value = 0;
  uint32_t current_value = 0;
  int32_t err;

  if (size < sizeof(NVCNT_DATA_TYPE))
  {
    return TFM_PLAT_ERR_INVALID_INPUT;
  }
  /* Search variable in active page and valid pages until erased page is found
     or in erasing pages until erased page is found */

  /* Set counter index to last element position in the page */
  counter = BL2_NV_COUNTERS_AREA_SIZE - NVCNT_ELEMENT_SIZE;

  /* Check each page address starting from end */
  while (counter >= PAGE_HEADER_SIZE)
  {
    /* Get the current location content to be compared with virtual address */
    err = FLASH_DEV_NAME.ReadData(BL2_NV_COUNTERS_AREA_ADDR + counter, &addressvalue,
                                  sizeof(addressvalue));

    if ((err == ARM_DRIVER_OK) && (addressvalue != NVCNT_PAGESTAT_ERASED))
    {
      /* check a zero is not written , zero is forbidden, zero can be used to clean a value  */
      if (addressvalue == 0)
      {
        return TFM_PLAT_ERR_SYSTEM_ERR;
      }

      /* Compare the read address with the virtual address */
      if (NVCNT_ID_VALUE(addressvalue) == counter_id)
      {
        /* Calculate crc of variable data and virtual address */
        crc = CalculateCrc(NVCNT_DATA_VALUE(addressvalue), NVCNT_ID_VALUE(addressvalue));

        /* if crc verification pass, data is correct and is returned.
           if crc verification fails, data is corrupted and has to be skip */
        if (crc == NVCNT_CRC_VALUE(addressvalue))
        {
          if (found != 1)
          {
            *((uint32_t *)val) = NVCNT_DATA_VALUE(addressvalue);
            previous_value = NVCNT_DATA_VALUE(addressvalue);
            found = 1;
          }
          else
          {

            current_value = NVCNT_DATA_VALUE(addressvalue);
            if (current_value >= previous_value)
            {
              return TFM_PLAT_ERR_SYSTEM_ERR;
            }
            else previous_value = current_value;
          }
        }
      }
    }
    /* Next address location */
    counter -= NVCNT_ELEMENT_SIZE;

  }
  if (found == 1)
  {
    return TFM_PLAT_ERR_SUCCESS;;
  }
  else
  {
    /* Variable is not found */
    return TFM_PLAT_ERR_SYSTEM_ERR;
  }
}
/**
  * @}
  */

/* Private functions ---------------------------------------------------------*/
/** @addtogroup EEPROM_Private_Functions
  * @{
  */


/**
  * @brief  Verify if specified page is fully erased.
  *         A page with double ECC error is consider as not fully erased
  * @param  Address page address
  * @param  PageSize page size
  * @retval bool
  *           - false : if Page not erased
  *           - true    : if Page erased
  */
static bool VerifyPageFullyErased(uint32_t Address, uint32_t PageSize)
{
  bool readstatus = true;
  uint32_t counter = 0U;
  int32_t err;
  NVCNT_ELEMENT_TYPE addressvalue;
  /* Check each element in the page */
  while (counter < PageSize)
  {
    err = FLASH_DEV_NAME.ReadData(Address + counter, &addressvalue,
                                  sizeof(addressvalue));

    if ((err != ARM_DRIVER_OK) || (addressvalue != NVCNT_PAGESTAT_ERASED))
    {
      /* In case one element is not erased, reset readstatus flag */
      readstatus = false;
    }
    /* Next address location */
    counter = counter + NVCNT_ELEMENT_SIZE;
  }

  /* Return readstatus value */
  return readstatus;
}


/**
  * @brief  This function configures CRC Instance.
  * @note   This function is used to :
  *         -1- Enable peripheral clock for CRC.
  *         -2- Configure CRC functional parameters.
  * @note   Peripheral configuration is minimal configuration from reset values.
  *         Thus, some useless LL unitary functions calls below are provided as
  *         commented examples - setting is default configuration from reset.
  * @param  None
  * @retval None
  */
static void ConfigureCrc(void)
{
  /* (1) Enable peripheral clock for CRC */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);

  /* (2) Configure CRC functional parameters */

  /* Configure CRC calculation unit with user defined polynomial */
  LL_CRC_SetPolynomialCoef(CRC, CRC_POLYNOMIAL_VALUE);
  LL_CRC_SetPolynomialSize(CRC, CRC_POLYNOMIAL_LENGTH);
}

/**
  * @brief  This function performs CRC calculation on Data and Virtual Address.
  * @param  Data value of  the eeprom variable.
  * @param  VirtAddress address of the eeprom variable.
  * @retval 16-bit CRC value computed on Data and Virtual Address.
  */
static uint16_t CalculateCrc(NVCNT_DATA_TYPE Data, uint16_t VirtAddress)
{

  /* Reset CRC calculation unit */
  LL_CRC_ResetCRCCalculationUnit(CRC);
  /* crc config */
  ConfigureCrc();

  /* Feed Data and Virtual Address */
  LL_CRC_FeedData32(CRC, Data);
  LL_CRC_FeedData16(CRC, VirtAddress);

  /* Return computed CRC value */
  return (LL_CRC_ReadData16(CRC));
}
/**
  * @brief  This function writes the header of NVM counter area.
  * @retval boolean return true on success
  */
static inline bool Write_Header(void)
{
  /*  a dummy header is set,  a random uint32_t with random id and a CRC is
   *  required for product */
  __IO NVCNT_DATA_TYPE huk[1];
  NVCNT_ID_TYPE huk_random_id = NVCNT_ID_HUK;
  int32_t err;
  uint32_t crc = 0U;
  __IO NVCNT_ELEMENT_TYPE element;
  int32_t loop = sizeof(huk) / sizeof(NVCNT_DATA_TYPE) - 1;
  uint32_t address = BL2_NV_COUNTERS_AREA_ADDR;
  huk[0] = NVCNT_HEADER_VALUE;
  /*  write the HUK in header */
  while (loop >= 0)
  {
    /* store in revert order , read is done in revert at the end all is in order */
    /* compute crc */
    crc = CalculateCrc(huk[loop], huk_random_id);
    /*  build element  */
    element = NVCNT_ELEMENT_VALUE(huk_random_id, huk[loop], crc);
    /* write element  */
    err = FLASH_DEV_NAME.ProgramData(address,
                                     (const void *)&element,
                                     NVCNT_ELEMENT_SIZE);
    if (err != ARM_DRIVER_OK)
    {
      break;
    }
    address += sizeof(NVCNT_ELEMENT_TYPE);
    loop--;
  }

  /* clean huk footprint  */
  element = 0;
  memset((void *)huk, 0, sizeof(huk));
  if (err == ARM_DRIVER_OK)
  {
    return true;
  }
  else return false;
}
/**
  * @brief  This function checks the header from NVM count area
  *
  * @retval boolean return true on success
  */
static bool Check_Header(void)
{
   NVCNT_ELEMENT_TYPE addressvalue;
  __IO NVCNT_DATA_TYPE huk[1];

  uint32_t  crc = 0U;
  int32_t err;
  bool ret = true;
  int32_t loop = sizeof(huk) / sizeof(NVCNT_DATA_TYPE) - 1;
  uint32_t address = BL2_NV_COUNTERS_AREA_ADDR;
  /* Check each page address starting from end */
  /* Get the current location content to be compared with virtual address */
  while (loop >= 0)
  {
    err = FLASH_DEV_NAME.ReadData(address, (void *)&addressvalue,
                                  sizeof(addressvalue));
    if ((err == ARM_DRIVER_OK) && (addressvalue != NVCNT_PAGESTAT_ERASED))
    {
      crc = CalculateCrc(NVCNT_DATA_VALUE(addressvalue), NVCNT_ID_VALUE(addressvalue));

      /* if crc verification pass, data is correct and is returned.
         if crc verification fails, data is corrupted and has to be skip */
      if (crc == NVCNT_CRC_VALUE(addressvalue))
      {
        /* recopy key in revert order  */
        huk[loop] = NVCNT_DATA_VALUE(addressvalue);
      }
      else
        ret = false;
    }
    address += sizeof(NVCNT_ELEMENT_TYPE);
    loop--;
  }
  if (huk[0] != NVCNT_HEADER_VALUE)
  {
    ret = false;
  }	
  return ret;
}
/**
  * @brief  init counter data in NV area.
  * @retval bool : true if success
                 false if failed
  */
static inline bool init_counter(void)
{
  int32_t err;
  uint32_t crc = 0U;
  NVCNT_ELEMENT_TYPE element;
  enum tfm_nv_counter_t counter_id;
  const uint32_t data = NVCNT_INIT_VALUE;

  /* initialize only one counter */
  for (counter_id = PLAT_NV_COUNTER_3; counter_id < PLAT_NV_COUNTER_MAX; counter_id++)
  {

    /* Check if pages are full, i.e. max number of written elements achieved */
    if (uhNbWrittenElements >= NVCNT_MAX_WRITTEN_ELEMENTS)
    {
      return false;
    }

    /* Calculate crc of variable data and virtual address */
    crc = CalculateCrc(data, counter_id);
    /*  build element  */
    element = NVCNT_ELEMENT_VALUE(counter_id, data, crc);
    /* Program variable data + virtual address + crc */
    err = FLASH_DEV_NAME.ProgramData(BL2_NV_COUNTERS_AREA_ADDR + uwAddressNextWrite,
                                     &element,
                                     NVCNT_ELEMENT_SIZE);
    /* If program operation was failed, a Flash error code is returned */
    if (err != ARM_DRIVER_OK)
    {
      return false;
    }

    /* Increment global variables relative to write operation done*/
    uwAddressNextWrite += NVCNT_ELEMENT_SIZE;
    uhNbWrittenElements++;
  }
  return true;
}
/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/