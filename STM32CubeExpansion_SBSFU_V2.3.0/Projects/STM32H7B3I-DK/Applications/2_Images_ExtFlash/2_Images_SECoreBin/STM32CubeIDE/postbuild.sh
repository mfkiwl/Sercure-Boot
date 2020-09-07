#!/bin/bash - 
#Post build for SECBOOT_ECCDSA_WITH_AES128_CTR_SHA256
# arg1 is the build directory
# arg2 is the elf file path+name
# arg3 is the bin file path+name
# arg4 is the version
# arg5 when present forces "bigelf" generation
projectdir=$1
FileName=${3##*/}
execname=${FileName%.*}
elf=$2
bin=$3
version=$4

SecureEngine=${0%/*} 

userAppBinary=$projectdir"/../Binary"

sfu=$userAppBinary"/"$execname".sfu"
sfb=$userAppBinary"/"$execname".sfb"
sign=$userAppBinary"/"$execname".sign"
headerbin=$userAppBinary"/"$execname"sfuh.bin"
bigbinary=$userAppBinary"/SBSFU_"$execname"_Header.bin"

nonce=$SecureEngine"/../Binary/nonce.bin"
oemkey=$SecureEngine"/../Binary/OEM_KEY_COMPANY1_key_AES_CTR.bin"
ecckey=$SecureEngine"/../Binary/ECCKEY.txt"
partialbin=$userAppBinary"/Partial"$execname".bin"
partialsfb=$userAppBinary"/Partial"$execname".sfb"
partialsfu=$userAppBinary"/Partial"$execname".sfu"
partialsign=$userAppBinary"/Partial"$execname".sign"
partialoffset=$userAppBinary"/Partial"$execname".offset"
ref_userapp=$projectdir"/RefUserApp.bin"

current_directory=`pwd`
cd  $SecureEngine"/../../"
SecureDir=`pwd`
cd $current_directory
sbsfuelf="$SecureDir/2_Images_SBSFU/STM32CubeIDE/STM32H7B3I_DISCO_2_Images_SBSFU/Debug/SBSFU.elf"
mapping="$SecureDir/Linker_Common/STM32CubeIDE/mapping_fwimg.ld"

current_directory=`pwd`
cd $1/../../../../../../Middlewares/ST/STM32_Secure_Engine/Utilities/KeysAndImages
basedir=`pwd`
cd $current_directory
# test if window executeable useable
prepareimage=$basedir"/win/prepareimage/prepareimage.exe"
uname | grep -i -e windows -e mingw > /dev/null 2>&1

if [ $? == 0 ] && [   -e "$prepareimage" ]; then
  echo "prepareimage with windows executeable"
  export PATH=$basedir"\win\prepareimage";$PATH > /dev/null 2>&1
  cmd=""
  prepareimage="prepareimage.exe"
else
  # line for python
  echo "prepareimage with python script"
  prepareimage=$basedir/prepareimage.py
  cmd="python"
fi

echo "$cmd $prepareimage" >> $1/output.txt
# Make sure we have a Binary sub-folder in UserApp folder
if [ ! -e $userAppBinary ]; then
mkdir $userAppBinary
fi

#Initialization vector AES_IV[127:0]= Nonce[63:0] || 0b0000 0000 0000 0000 0000 0000 0000 0000 0000 || Address[31:4] (address modulo 128-bit)
#Address[31:4] == ((__ICFEDIT_region_SLOT_0_start__ + FW_OFFSET_IMAGE) >> 4) ==> -a 0x09000040
set "command=%python%%prepareimage% enc -k %oemkey% -n %nonce% -a 0x09000040 %bin% %sfu%  > %projectdir%\output.txt 2>&1"

command=$cmd" "$prepareimage" enc -k "$oemkey" -n "$nonce" -a 0x09000040 "$bin" "$sfu
$command > "$projectdir"/output.txt
ret=$?
if [ $ret == 0 ]; then
  command=$cmd" "$prepareimage" sha256 "$sfu" "$sign
  $command >> $projectdir"/output.txt"
  ret=$?
  if [ $ret == 0 ]; then 
    command=$cmd" "$prepareimage" pack -k "$ecckey" -r 36 -v "$version" -n "$nonce" -f "$sfu" -t "$sign" "$sfb" -o 1024"
    $command >> $projectdir"/output.txt"
    ret=$?
    if [ $ret == 0 ]; then
      command=$cmd" "$prepareimage" header -k  "$ecckey" -r 36 -v "$version"  -i "$nonce" -f "$sfu" -t "$sign" -o 1024 "$headerbin
      $command >> $projectdir"/output.txt"
      ret=$?
      if [ $ret == 0 ]; then
        command=$cmd" "$prepareimage" extract -d __ICFEDIT_region_SLOT_0_header__ "$mapping
        header=`$command`
        ret=$?
        echo "header $header"
        echo "header $header" >> $projectdir"/output.txt"
        if [ $ret == 0 ]; then
          command=$cmd" "$prepareimage" merge -v 0 -e 1 -i "$headerbin" -s "$sbsfuelf" -x "$header" "$bigbinary
          $command >> $projectdir"/output.txt"
          ret=$?
          #Partial image generation if reference userapp exists
          if [ $ret == 0 ] && [ -e "$ref_userapp" ]; then
            echo "Generating the partial image .sfb"
            echo "Generating the partial image .sfb" >> $projectdir"/output.txt"
            command=$cmd" "$prepareimage" diff -1 "$ref_userapp" -2 "$bin" "$partialbin" -a 16 --poffset "$partialoffset
            $command >> $projectdir"/output.txt"
            ret=$?
            if [ $ret == 0 ]; then
              command=$cmd" "$prepareimage" enc -k "$oemkey" -n "$nonce" -a 0x09000040 --poffset "$partialoffset" "$partialbin" "$partialsfu
              $command >> $projectdir"/output.txt"
              ret=$?
              if [ $ret == 0 ]; then
                command=$cmd" "$prepareimage" sha256 "$partialsfu" "$partialsign
                $command >> $projectdir"/output.txt"
                ret=$?
                if [ $ret == 0 ]; then
                  command=$cmd" "$prepareimage" pack -k "$ecckey" -r 36 -v "$version" -n "$nonce" -f "$sfu" -t "$sign" -o 1024 --pfw "$partialsfu" --ptag "$partialsign" --poffset  "$partialoffset" "$partialsfb
                  $command >> $projectdir"/output.txt"
                  ret=$?
                fi
              fi
            fi
          fi
        fi
        if [ $ret == 0 ] && [ $# == 5 ]; then
          echo "Generating the global elf file SBSFU and userApp"
          echo "Generating the global elf file SBSFU and userApp" >> $projectdir"/output.txt"
          uname | grep -i -e windows -e mingw > /dev/null 2>&1
          if [ $? == 0 ]; then
            # Set to the default installation path of the Cube Programmer tool
            # If you installed it in another location, please update PATH.
            PATH="C:\\Program Files\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\bin";$PATH > /dev/null 2>&1
            programmertool="STM32_Programmer_CLI.exe"
          else
            echo "fix access path to STM32_Programmer_CLI.exe "
          fi
          command=$programmertool" -ms "$elf" "$headerbin" "$sbsfuelf
          $command >> $projectdir"/output.txt"
          ret=$?
        fi
      fi
    fi
  fi
fi

if [ $ret == 0 ]; then
  rm $sign
  rm $sfu
  rm $headerbin
  if [ -e "$ref_userapp" ]; then
    rm $partialbin
    rm $partialsfu
    rm $partialsign
    rm $partialoffset
  fi  
  exit 0
else 
  echo "$command : failed" >> $projectdir"/output.txt"
  if [ -e  "$elf" ]; then
    rm  $elf
  fi
  if [ -e "$elfbackup" ]; then 
    rm  $elfbackup
  fi
  echo $command : failed
  read -n 1 -s
  exit 1
fi