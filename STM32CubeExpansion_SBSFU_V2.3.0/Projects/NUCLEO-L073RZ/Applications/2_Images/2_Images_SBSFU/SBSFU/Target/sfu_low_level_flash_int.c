/**
  ******************************************************************************
  * @file    sfu_low_level_flash_int.c
  * @author  MCD Application Team
  * @brief   SFU Flash Low Level Interface module
  *          This file provides set of firmware functions to manage SFU internal
  *          flash low level interface.
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
#include "main.h"
#include "sfu_low_level_flash.h"
#include "sfu_low_level_security.h"
#include "se_interface_bootloader.h"
#include "string.h"


/** @addtogroup SFU Secure Boot / Secure Firmware Update
  * @{
  */

/** @addtogroup  SFU_LOW_LEVEL
  * @{
  */

/** @defgroup SFU_LOW_LEVEL_FLASH Flash Low Level Interface
  * @{
  */

/** @defgroup SFU_FLASH_Private_Definition Private Definitions
  * @{
  */
#define NB_PAGE_SECTOR_PER_ERASE  2U    /*!< Nb page erased per erase */

/**
  * @}
  */
/** @defgroup SFU_FLASH_Private_Variables Private Variables
  * @{
  */


/**
  * @}
  */

/** @defgroup SFU_FLASH_Private_Functions Private Functions
  * @{
  */
static SFU_ErrorStatus SFU_LL_FLASH_INT_Clear_Error(void);

/**
  * @}
  */

/** @addtogroup SFU_FLASH_Exported_Functions
  * @{
  */

/**
  * @brief  This function initialize the internal flash interface if required
  * @param  none
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_FLASH_INT_Init(void)
{
  return SFU_SUCCESS;
}

/**
  * @brief  This function does an erase of n (depends on Length) pages in user flash area
  * @param  pFlashStatus: SFU_FLASH Status pointer
  * @param  pStart: flash address to be erased
  * @param  Length: number of bytes
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_FLASH_INT_Erase_Size(SFU_FLASH_StatusTypeDef *pFlashStatus, void *pStart, uint32_t Length)
{
  uint32_t page_error = 0U;
  uint32_t start = (uint32_t)pStart;
  FLASH_EraseInitTypeDef p_erase_init;
  SFU_ErrorStatus e_ret_status = SFU_SUCCESS;
  uint32_t first_page = 0U;
  uint32_t nb_pages = 0U;
  uint32_t chunk_nb_pages;

  /* Check the pointers allocation */
  if (pFlashStatus == NULL)
  {
    return SFU_ERROR;
  }

  /* Test if access is in this range : SLOT 0 header */
  if ((Length != 0) && ((uint32_t)pStart >= SFU_IMG_SLOT_0_REGION_HEADER_VALUE) &&
      ((((uint32_t)pStart + Length - 1)) < (SFU_IMG_SLOT_0_REGION_HEADER_VALUE + SFU_IMG_IMAGE_OFFSET))
     )
  {
    /* SE Access */
    SE_StatusTypeDef se_status;
    SE_ErrorStatus se_ret_status = SE_SFU_IMG_Erase(&se_status, pStart, Length);
    if (se_ret_status == SE_SUCCESS)
    {
      e_ret_status = SFU_SUCCESS;
      *pFlashStatus = SFU_FLASH_SUCCESS;
    }
    else
    {
      e_ret_status = SFU_ERROR;
      *pFlashStatus = SFU_FLASH_ERROR;
    }
  }
  else
  {
    *pFlashStatus = SFU_FLASH_SUCCESS;

    /* Clear error flags raised during previous operation */
    e_ret_status = SFU_LL_FLASH_INT_Clear_Error();

    if (e_ret_status == SFU_SUCCESS)
    {
      /* Unlock the Flash to enable the flash control register access *************/
      if (HAL_FLASH_Unlock() == HAL_OK)
      {
        first_page = SFU_LL_FLASH_INT_GetPage(start);
        /* Get the number of pages to erase from 1st page */
        nb_pages = SFU_LL_FLASH_INT_GetPage(start + Length - 1U) - first_page + 1U;
        p_erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
        /* Erase flash per NB_PAGE_SECTOR_PER_ERASE to avoid watch-dog */
        do
        {
          chunk_nb_pages = (nb_pages >= NB_PAGE_SECTOR_PER_ERASE) ? NB_PAGE_SECTOR_PER_ERASE : nb_pages;
          p_erase_init.PageAddress = first_page * FLASH_PAGE_SIZE + FLASH_BASE;
          p_erase_init.NbPages = chunk_nb_pages;
          first_page += chunk_nb_pages;
          nb_pages -= chunk_nb_pages;
          if (HAL_FLASHEx_Erase(&p_erase_init, &page_error) != HAL_OK)
          {
            e_ret_status = SFU_ERROR;
            *pFlashStatus = SFU_FLASH_ERR_ERASE;
          }
          SFU_LL_SECU_IWDG_Refresh(); /* calling this function which checks the compiler switch */
        } while (nb_pages > 0);
        /* Lock the Flash to disable the flash control register access (recommended
        to protect the FLASH memory against possible unwanted operation) *********/
        HAL_FLASH_Lock();
      }
      else
      {
        *pFlashStatus = SFU_FLASH_ERR_HAL;
      }
    }
  }

  return e_ret_status;
}

/**
  * @brief  This function writes a data buffer in flash (data are 32-bit aligned).
  * @note   After writing data buffer, the flash content is checked.
  * @param  pFlashStatus: FLASH_StatusTypeDef
  * @param  pDestination: Start address for target location. It has to be 4 bytes aligned.
  * @param  pSource: pointer on buffer with data to write
  * @param  Length: Length of data buffer in bytes. It has to be 4 bytes aligned.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_FLASH_INT_Write(SFU_FLASH_StatusTypeDef *pFlashStatus, void  *pDestination, const void *pSource,
                                       uint32_t Length)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
  uint32_t i = 0U;
  uint32_t pdata = (uint32_t)pSource;

  /* Check the pointers allocation */
  if ((pFlashStatus == NULL) || (pSource == NULL))
  {
    return SFU_ERROR;
  }
  /* Test if access is in this range : SLOT 0 header */
  if ((Length != 0) && ((uint32_t)pDestination >= SFU_IMG_SLOT_0_REGION_HEADER_VALUE) &&
      ((((uint32_t)pDestination + Length - 1)) < (SFU_IMG_SLOT_0_REGION_HEADER_VALUE + SFU_IMG_IMAGE_OFFSET))
     )
  {
    /* SE Access */
    SE_StatusTypeDef se_status;
    SE_ErrorStatus se_ret_status = SE_SFU_IMG_Write(&se_status, pDestination, pSource, Length);
    if (se_ret_status == SE_SUCCESS)
    {
      e_ret_status = SFU_SUCCESS;
      *pFlashStatus = SFU_FLASH_SUCCESS;
    }
    else
    {
      e_ret_status = SFU_ERROR;
      *pFlashStatus = SFU_FLASH_ERROR;
    }
  }
  else
  {
    *pFlashStatus = SFU_FLASH_ERROR;

    /* Clear error flags raised during previous operation */
    e_ret_status = SFU_LL_FLASH_INT_Clear_Error();

    if (e_ret_status == SFU_SUCCESS)
    {
      /* Unlock the Flash to enable the flash control register access *************/
      if (HAL_FLASH_Unlock() != HAL_OK)
      {
        *pFlashStatus = SFU_FLASH_ERR_HAL;

      }
      else
      {
        for (i = 0U; i < Length; i += sizeof(SFU_LL_FLASH_write_t))
        {
          *pFlashStatus = SFU_FLASH_ERROR;
          if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)pDestination, *((uint32_t *)(pdata + i))) == HAL_OK)
          {
            /* Check the written value */
            if (*(uint32_t *)pDestination != *(uint32_t *)(pdata + i))
            {
              /* Flash content doesn't match SRAM content */
              *pFlashStatus = SFU_FLASH_ERR_WRITINGCTRL;
              e_ret_status = SFU_ERROR;
              break;
            }
            else
            {
              /* Increment FLASH Destination address */
              pDestination = (uint8_t *)pDestination + sizeof(SFU_LL_FLASH_write_t);
              e_ret_status = SFU_SUCCESS;
              *pFlashStatus = SFU_FLASH_SUCCESS;
            }
          }
          else
          {
            /* Error occurred while writing data in Flash memory */
            *pFlashStatus = SFU_FLASH_ERR_WRITING;
            e_ret_status = SFU_ERROR;
            break;
          }
        }
        /* Lock the Flash to disable the flash control register access (recommended
        to protect the FLASH memory against possible unwanted operation) */
        HAL_FLASH_Lock();
      }
    }
  }
  return e_ret_status;
}

/**
  * @brief  This function reads flash
  * @param  pDestination: Start address for target location
  * @param  pSource: flash address to read
  * @param  Length: number of bytes
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_FLASH_INT_Read(void *pDestination, const void *pSource, uint32_t Length)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Test if access is in this range : SLOT 0 header */
  if (((uint32_t)pSource >= SFU_IMG_SLOT_0_REGION_HEADER_VALUE) &&
      ((((uint32_t)pSource + Length - 1)) < (SFU_IMG_SLOT_0_REGION_HEADER_VALUE + SFU_IMG_IMAGE_OFFSET))
     )
  {
    /* SE Access */
    SE_StatusTypeDef se_status;
    SE_ErrorStatus se_ret_status = SE_SFU_IMG_Read(&se_status, pDestination, pSource, Length);
    if (se_ret_status == SE_SUCCESS)
    {
      e_ret_status = SFU_SUCCESS;
    }
  }
  else
  {
    memcpy(pDestination, pSource, Length);
    e_ret_status = SFU_SUCCESS;
  }
  return e_ret_status;
}

/**
  * @brief  Gets the page of a given address
  * @param  Addr: flash address
  * @retval The page of a given address
  */
uint32_t SFU_LL_FLASH_INT_GetPage(uint32_t Addr)
{
  uint32_t page = 0U;

  page = (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;

  return page;
}


/**
  * @brief  Gets the sector of a given address
  * @param  Add: flash address
  * @retval The sector of a given address
  */
uint32_t SFU_LL_FLASH_INT_GetSector(uint32_t Add)
{
  uint32_t sector = 0U;

  sector = (Add - FLASH_BASE) / (FLASH_PAGE_SIZE * 32U);

  return sector;
}


/**
  * @}
  */

/** @defgroup SFU_FLASH_Private_Functions Private Functions
  * @{
  */

/**
  * @brief  Clear error flags raised during previous operation
  * @param  None.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus SFU_LL_FLASH_INT_Clear_Error(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Unlock the Program memory */
  if (HAL_FLASH_Unlock() == HAL_OK)
  {

    /* Clear all FLASH flags */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR | FLASH_FLAG_OPTVERR |
                           FLASH_FLAG_RDERR  | FLASH_FLAG_WRPERR | FLASH_FLAG_FWWERR |
                           FLASH_FLAG_NOTZEROERR);

    /* Unlock the Program memory */
    if (HAL_FLASH_Lock() == HAL_OK)
    {
      e_ret_status = SFU_SUCCESS;
    }
  }

  return e_ret_status;

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

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
