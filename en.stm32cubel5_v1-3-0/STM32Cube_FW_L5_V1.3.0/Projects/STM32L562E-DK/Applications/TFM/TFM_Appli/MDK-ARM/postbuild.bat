:: arg1 is the build directory
:: arg2 is the version
:: arg3 is the binary type (1 nonsecure, 2 secure)
set "projectdir=%1"
set "version=%2"
set "signing=%~3"
set "userAppBinary=%projectdir%\..\Binary"

set "tfm_ns_sign=%userAppBinary%\tfm_ns_sign.bin"
set "tfm_s_sign=%userAppBinary%\tfm_s_sign.bin"
set "tfm_s_enc_sign=%userAppBinary%\tfm_s_enc_sign.bin"
set "tfm_ns_enc_sign=%userAppBinary%\tfm_ns_enc_sign.bin"
:: variable for image_number = 1
set "tfm_enc_sign=%userAppBinary%\tfm_enc_sign.bin"
set "tfm_sign=%userAppBinary%\tfm_sign.bin"
set "tfm=%userAppBinary%\tfm.bin"

::field updated with tfm_sbsfu_boot postbuild

set image_ns_size=0x20000
set image_s_size=0x29000
set image_number=2
set crypto_scheme=0
set external_flash_enable=0
set ns_code_start=0x0
:: end of updated field

pushd %projectdir%\..\..\..\..\..\..\Middlewares\Third_Party\trustedfirmware\bl2\ext\mcuboot
set tfm_mcuboot_dir=%cd%
popd
pushd %projectdir%\..\..\..\..\..\..\Middlewares\Third_Party\mcuboot
set mcuboot_dir=%cd%
popd
::Make sure we have a Binary sub-folder in UserApp folder
if not exist "%userAppBinary%" (
mkdir "%userAppBinary%"
)
IF "%signing%" == "nonsecure" (
goto :start;
)

if  "%image_number%" == "1" (
:: when image number is 1, image is assemble and sign during non secure postbuild
exit 0
)
:start
goto exe:
goto py:
:exe
::line for window executeable
echo Postbuild with windows executable
set "imgtool=%mcuboot_dir%\scripts\dist\imgtool\imgtool.exe"
set "python="
if exist %imgtool% (
goto postbuild
)
:py
::line for python
echo Postbuild with python script
set "imgtool=%mcuboot_dir%\scripts\imgtool.py"
set "python=python "
:postbuild


::sign mode
if "%crypto_scheme%" == "0" (
set "mode=rsa2048"
)
if "%crypto_scheme%" == "1" (
set "mode=rsa3072"
)
if "%crypto_scheme%" == "2" (
set "mode=ec256"
)
::keys selection
if  "%mode%" == "rsa2048" (
set "key_s=%tfm_mcuboot_dir%\root-rsa-2048.pem"
set "key_ns=%tfm_mcuboot_dir%\root-rsa-2048_1.pem"
set "key_enc_pub=%tfm_mcuboot_dir%\enc-rsa2048-pub.pem"
)
if  "%mode%" == "rsa3072" (
set "key_s=%tfm_mcuboot_dir%\root-rsa-3072.pem"
set "key_ns=%tfm_mcuboot_dir%\root-rsa-3072_1.pem"
set "key_enc_pub=%tfm_mcuboot_dir%\enc-rsa2048-pub.pem"
)
if  "%mode%" == "ec256" (
set "key_s=%tfm_mcuboot_dir%\root-ec-p256.pem"
set "key_ns=%tfm_mcuboot_dir%\root-ec-p256_1.pem"
set "key_enc_pub=%tfm_mcuboot_dir%\enc-ec256-pub.pem"
)
set "tfm_s=%projectdir%\STM32L562E-DK_S\Exe\Project.bin"
set "tfm_ns=%projectdir%\STM32L562E-DK_NS\Exe\Project.bin"

:signing
echo %signing% %mode% %option% image_number=%image_number% > %projectdir%\output.txt 2>&1

if "%image_number%" == "2" (
goto continue
)
echo "assemble image" >> %projectdir%\output.txt 2>&1
set "command_ass=%python%%imgtool% ass -f %tfm_s% -o %image_s_size%  -i 0x0 %tfm_ns% %tfm% >> %projectdir%\output.txt 2>&1"
%command_ass%
IF %ERRORLEVEL% NEQ 0 goto :error_ass
:: sign assemble binary as secure image, slot size is image_ns_size
set "image_s_size=%image_ns_size%"
set "signing="
set "tfm_s=%tfm%"
set "tfm_s_sign=%tfm_sign%"
set "tfm_s_enc_sign=%tfm_enc_sign%"
:continue

IF "%signing%" == "nonsecure" (
echo "%signing% signing" >> %projectdir%\output.txt 2>&1
IF "%external_flash_enable%" == "1" (
set "command_sign=%python%%imgtool% sign -k %key_ns% -E %key_enc_pub% -o %ns_code_start% -e little -S %image_ns_size% -H 0x400 --pad-header %option% -v %version% -s auto --align 8 %tfm_ns% %tfm_ns_sign% >> %projectdir%\output.txt 2>&1"
) ELSE (
set "command_sign=%python%%imgtool% sign -k %key_ns% -e little -S %image_ns_size% -H 0x400 --pad-header %option% -v %version% -s auto --align 8 %tfm_ns% %tfm_ns_sign% >> %projectdir%\output.txt 2>&1"
)
goto :docommand_sign
)
echo "%signing% signing" >> %projectdir%\output.txt 2>&1
set "command_sign=%python%%imgtool% sign -k %key_s% -e little -S %image_s_size% -H 0x400 --pad-header %option% -v %version% -s auto --align 8 %tfm_s% %tfm_s_sign% >> %projectdir%\output.txt 2>&1"
:docommand_sign
%command_sign%
IF %ERRORLEVEL% NEQ 0 goto :error
IF "%signing%" == "nonsecure" (
echo "non secure encrypting" >> %projectdir%\output.txt 2>&1
IF "%external_flash_enable%" == "1" (
set "command_enc=%python%%imgtool% sign -k %key_ns% -E %key_enc_pub% -o %ns_code_start% -e little -S %image_ns_size% -H 0x400 --pad-header %option% -v %version%  -s auto --align 8 %tfm_ns% %tfm_ns_enc_sign% >> %projectdir%\output.txt 2>&1"
) ELSE (
set "command_enc=%python%%imgtool% sign -k %key_ns% -E %key_enc_pub% -e little -S %image_ns_size% -H 0x400 --pad-header %option% -v %version%  -s auto --align 8 %tfm_ns% %tfm_ns_enc_sign% >> %projectdir%\output.txt 2>&1"
)
goto :docommand_enc
)
echo "secure encrypting" >> %projectdir%\output.txt 2>&1
set "command_enc=%python%%imgtool% sign -k %key_s% -E %key_enc_pub% -e little -S %image_s_size% -H 0x400 --pad-header %option% -v %version%  -s auto --align 8 %tfm_s% %tfm_s_enc_sign% >> %projectdir%\output.txt 2>&1"
:docommand_enc
%command_enc%
IF %ERRORLEVEL% NEQ 0 goto :error_enc
exit 0
:error
echo "%command_sign% : failed" >> %projectdir%\\output.txt
exit 1
:error_enc
echo "%command_enc% : failed" >> %projectdir%\\output.txt
exit 1
:error_ass
echo "%command_ass% : failed" >> %projectdir%\\output.txt
exit 1

