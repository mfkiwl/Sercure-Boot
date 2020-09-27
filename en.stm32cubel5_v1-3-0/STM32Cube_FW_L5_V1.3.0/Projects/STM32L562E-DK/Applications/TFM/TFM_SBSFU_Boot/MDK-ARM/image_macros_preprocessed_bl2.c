









enum image_attributes
{

  RE_IMAGE_FLASH_SECURE_IMAGE_SIZE = ((0x29000)),

  RE_IMAGE_FLASH_NON_SECURE_IMAGE_SIZE = ((0x20000)),




  RE_IMAGE_FLASH_ADDRESS_SECURE = ((0x0c000000)+(((((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))+(0x800))+(0x1000)) + (0x2000))+(0x2000))),
  RE_EXTERNAL_FLASH_ENABLE = (0x0),

  RE_IMAGE_NUMBER = (0x2),
  RE_CODE_START_NON_SECURE = (0x0),
  RE_IMAGE_FLASH_ADDRESS_NON_SECURE = ((0x0c000000)+((((((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))+(0x800))+(0x1000)) + (0x2000))+(0x2000)) + ((0x29000)))),


  RE_IMAGE_FLASH_SECURE_UPDATE = ((0x0c000000)+(((((((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))+(0x800))+(0x1000)) + (0x2000))+(0x2000)) + ((0x29000))) + ((0x20000)))),
  RE_IMAGE_FLASH_NON_SECURE_UPDATE = ((0x0c000000)+((((((((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))+(0x800))+(0x1000)) + (0x2000))+(0x2000)) + ((0x29000))) + ((0x20000))) + (0x8000))),
  RE_PRIMARY_ONLY = (0x0),
  RE_BL2_PERSO_ADDRESS = ((0x0c000000)+(((0x0000)+(0x1000))+(0x0000))),
  RE_BL2_BOOT_ADDRESS = ((0x0c000000)+((((0x0000)+(0x1000))+(0x0000))+(0x1000))),
  RE_BL2_SEC1_END = ((0x40000)-0x1),
  RE_BL2_SEC2_START = 0x0,
  RE_BL2_SEC2_END = ((((((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))+(0x800))+(0x1000)) + (0x2000))+(0x2000))+(0x29000)-(0x40000)-0x1)

  RE_BL2_WRP_START = ((((0x0000)+(0x1000))+(0x0000))),
  RE_BL2_WRP_END = ((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))+(0x800)-0x1),
  RE_BL2_HDP_END = ((((((0x0000)+(0x1000))+(0x0000))+(0x1000))+(0x20000))-0x1),
  RE_IMAGE_FLASH_LOADER_ADDRESS = (0x0),
  RE_LOADER_WRP_START = (0x7f),
  RE_LOADER_WRP_END = (0x0),
  RE_EXT_LOADER = (0x0),

  RE_CRYPTO_SCHEME = 0x0,
};
