/**
  ******************************************************************************
  * @file    test_protections.c
  * @author  MCD Application Team
  * @brief   Test Protections module.
  *          This file provides set of firmware functions to manage Test Protections
  *          functionalities.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
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
#include "test_protections.h"
#include "se_def.h"
#include "se_interface_application.h"
#include "com.h"
#include "common.h"
#include "flash_if.h"
#include "sfu_fwimg_regions.h" /* required for corruption tests (a real user application should NOT need this file) */
#if defined (__ICCARM__) || defined(__GNUC__)
#include "mapping_export.h"
#elif defined(__CC_ARM)
#include "mapping_sbsfu.h"
#endif /* __ICCARM__ || __GNUC__ */
/** @addtogroup USER_APP User App Example
  * @{
  */

/** @addtogroup TEST_PROTECTIONS Test protections
  * @{
  */

/** @defgroup  TEST_PROTECTIONS_Private_Defines Private Defines
  * @{
  */

/**
  * @brief  Isolated enclave Test.
  */
/*!< Address used to test SE CODE protection*/
#define TEST_PROTECTIONS_SE_ISOLATED_CODE_READKEY_ADDRESS   ((uint32_t) SE_KEY_REGION_ROM_START)
/*!< Address used to test SE VDATA protection*/
#define TEST_PROTECTIONS_SE_ISOLATED_VDATA_SRAM_ADDRESS     ((uint32_t) SE_REGION_RAM_START)

/**
  * @brief  PCROP Test.
  */
/*!< Address used to test PCROP protection*/
#define TEST_PROTECTIONS_PCROP_FLASH_ADDRESS          ((uint32_t) SE_KEY_REGION_ROM_START)
/*!< Size used to test PCROP AREA protection (Bytes)*/
#define TEST_PROTECTIONS_PCROP_SIZE                   ((uint32_t)64)
/**
  * @brief  WRP Test.
  */
/*!< Adress used to test WRP protection */
#define TEST_PROTECTIONS_WRP_FLASH_ADDRESS            ((uint32_t) SB_REGION_ROM_START)
/*!< WRP Test Size */
#define TEST_PROTECTIONS_WRP_FLASH_SIZE               ((uint32_t)0x800U)

/**
  * @brief  IWDG Test.
  */
/*!< IWDG Test delay in ms (it has to be greater than what used in SB)*/
#define TEST_PROTECTIONS_IWDG_DELAY                   ((uint32_t)7000U)

/**
  * @brief  TAMPER Test.
  */
#define TEST_PROTECTIONS_TAMPER_DELAY                 ((uint32_t)10U)         /*!< TAMPER Test delay in s */

/**
  * @brief  CORRUPT_IMAGE Test.
  */
/*!< CORRUPT_IMAGE Test: address where data will be corrupted: adress of slot 0 + offset */
#define TEST_PROTECTIONS_CORRUPT_IMAGE_FLASH_ADDRESS  ((uint32_t)(SFU_IMG_SLOT_0_REGION_BEGIN_VALUE \
                                                                  +SFU_IMG_IMAGE_OFFSET))
/*!< CORRUPT_IMAGE Test: size of data to be corrupted */
#define TEST_PROTECTIONS_CORRUPT_IMAGE_FLASH_SIZE     ((uint32_t)32U)
/**
  * @}
  */

/** @defgroup  TEST_PROTECTIONS_Private_Variables Private Variables
  * @{
  */
static void (*SE_ReadKey)(unsigned char *key);
static __IO uint32_t m_uTamperEvent = 0U;                /*!< Tamper Event */
/**
  * @}
  */

/** @defgroup  TEST_PROTECTIONS_Private_Functions Private Functions
  * @{
  */
static void TEST_PROTECTIONS_Run_SecUserMem_CODE(void);
static void TEST_PROTECTIONS_Run_TAMPER(void);
static void TEST_PROTECTIONS_Run_IWDG(void);
static void TEST_PROTECTIONS_Run_CORRUPT_IMAGE(void);
static void TEST_PROTECTIONS_PrintTestingMenu(void);
/**
  * @}
  */



/** @defgroup  TEST_PROTECTIONS_Exported_Functions Exported Functions
  * @{
  */

/** @defgroup  TEST_PROTECTIONS_Control_Functions Control Functions
  * @{
  */

/**
  * @brief  Display the TEST Main Menu choices on HyperTerminal
  * @param  None.
  * @retval None.
  */

void TEST_PROTECTIONS_Run(void)
{
  uint8_t key = 0U;
  uint8_t exit = 0U;

  /* Print Main Menu message*/
  TEST_PROTECTIONS_PrintTestingMenu();

  while (exit == 0U)
  {
    key = 0U;

    /* If the SecureBoot configured the IWDG, UserApp must reload IWDG counter with value defined in the reload
       register */
    WRITE_REG(IWDG1->KR, IWDG_KEY_RELOAD);

    /* Clean the input path */
    COM_Flush();

    /* Receive key */
    if (COM_Receive(&key, 1U, RX_TIMEOUT) == HAL_OK)
    {
      switch (key)
      {
        case '1' :
          TEST_PROTECTIONS_Run_SecUserMem_CODE();
          break;
        case '2' :
          TEST_PROTECTIONS_Run_IWDG();
          break;
        case '3' :
          TEST_PROTECTIONS_Run_TAMPER();
          break;
        case '4' :
          TEST_PROTECTIONS_Run_CORRUPT_IMAGE();
          break;
        case 'x' :
          exit = 1U;
          break;

        default:
          printf("Invalid Number !\r");
          break;
      }

      /*Print Main Menu message*/
      TEST_PROTECTIONS_PrintTestingMenu();
    }
  }
}


/**
  * @}
  */

/**
  * @}
  */

/** @addtogroup  TEST_PROTECTIONS_Private_Functions
  * @{
  */


/**
  * @brief  Display the TEST Main Menu choices on HyperTerminal
  * @param  None.
  * @retval None.
  */
static void TEST_PROTECTIONS_PrintTestingMenu(void)
{
  printf("\r\n=================== Test Menu ============================\r\n\n");
  printf("  Test Protection: Secure User memory ------------------- 1\r\n\n");
  printf("  Test Protection: IWDG --------------------------------- 2\r\n\n");
  printf("  Test Protection: TAMPER ------------------------------- 3\r\n\n");
  printf("  Test : CORRUPT ACTIVE IMAGE --------------------------- 4\r\n\n");
  printf("  Previous Menu ----------------------------------------- x\r\n\n");
}

/**
  * @brief  TEST Run Secure User Memory
  * @param  None.
  * @retval None.
  */
static void TEST_PROTECTIONS_Run_SecUserMem_CODE(void)
{
  /* 128 bit key + 1 char for NULL-terminated string */
  unsigned char key[17U];
  printf("\r\n====== Test Protection: Secure User Memory =================\r\n\n");
  printf("If the Secure User Memory is enabled you should not be able to read the key and get stuck.\r\n\n");
  printf("  -- Reading Key\r\n\n");
  printf("Press the RESET button to restart the device (or wait until IWDG expires if enabled).\r\n\n");

  SE_ReadKey = (void (*)(unsigned char *))((unsigned char *)(TEST_PROTECTIONS_SE_ISOLATED_CODE_READKEY_ADDRESS) + 1U);

  /* Executing Read Key Code which is located in Secure User memory */
  SE_ReadKey(&(key[0U]));
  /* Add the string termination to have a proper display */
  key[16] = '\0';

  /* Should not get here if Secure User Memory is available and enabled  */
  printf("  -- Key: %s \r\n\n", key);
  printf("  -- !! Secure User Memory protection is NOT ENABLED !!\r\n\n");
}



/**
  * @brief  TEST Run CORRUPT_IMAGE
  * @param  None.
  * @retval None.
  */
static void TEST_PROTECTIONS_Run_CORRUPT_IMAGE(void)
{

  printf("\r\n====== Test: CORRUPT ACTIVE IMAGE ======================\r\n\n");

  printf("  -- !! Not possible to corrupt external FLASH during UserApp execution \r\n");
  printf("     write and erase flash commands are not allowed in memory map mode configuration ...\r\n\n");
}


/**
  * @brief  TEST Run TAMPER
  * @param  None.
  * @retval None.
  */
static void TEST_PROTECTIONS_Run_TAMPER(void)
{
  if (m_uTamperEvent) ;   /* avoid warning unused variable */

  printf("\r\n====== Test Protection: TAMPER ========================\r\n\n");
  printf("  -- !! TAMPER protection is NOT ACTIVATED on this board due to conflict with USER button !!\r\n\n");
}

/**
  * @brief  TEST Run IWDG
  * @param  None.
  * @retval None.
  */
static void TEST_PROTECTIONS_Run_IWDG(void)
{
  printf("\r\n====== Test Protection: IWDG ===========================\r\n\n");

  /* Wait for TEST_PROTECTIONS_IWDG_DELAY*/
  printf("  -- Waiting %d (ms). Should reset if IWDG is enabled. \r\n\n", TEST_PROTECTIONS_IWDG_DELAY);

  HAL_Delay(TEST_PROTECTIONS_IWDG_DELAY);

  /* No Reset means IWDG was not enabled*/
  printf("  -- !! IWDG protection is NOT ENABLED !!\r\n\n");
}

/**
  * @}
  */

/** @defgroup TEST_PROTECTIONS_Callback_Functions Callback Functions
  * @{
  */

/**
  * @brief  Implement the Cube_Hal Callback generated on the Tamper IRQ.
  * @param  None
  * @retval None
  */
void CALLBACK_Antitamper(void)
{
  /*Set tamper event variable*/
  m_uTamperEvent = 1U;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
