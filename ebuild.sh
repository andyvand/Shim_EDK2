#!/bin/bash

#  ebuild.sh ->ebuild.sh  //renamed to be unique file begining from E
#  Script for building CloverEFI source under OS X
#  Supported chainloads(compilers) are XCODE32, UNIXGCC and CLANG
#  
#
#  Created by Jadran Puharic on 1/6/12.
#  Modified by JrCs on 3/9/13.

# Global variables
declare -r SELF="${0##*/}"
declare -r NUMBER_OF_CPUS=2
declare -a EDK2_BUILD_OPTIONS=
print_option_help_wc=
have_fmt=
PLATFORMFILE=
TARGETRULE=


# Default values
export TOOLCHAIN=GCC49
export TARGETARCH=X64
export BUILDTARGET=RELEASE
export BUILDTHREADS=$(( NUMBER_OF_CPUS + 1 ))
export WORKSPACE=${WORKSPACE:-}
export TOOLCHAIN_DIR=${TOOLCHAIN_DIR:-~/src/opt/local}

VBIOSPATCHCLOVEREFI=0
ONLYSATA0PATCH=0
USE_BIOS_BLOCKIO=0

# Bash options
set -e # errexit
set -u # Blow on unbound variable

if [[ -x /usr/bin/git ]]; then
    PATCH_CMD="/usr/bin/git apply --whitespace=nowarn"
else
    PATCH_CMD="/usr/bin/patch"
fi

# Check if we need to patch the sources
PATCH_FILE=

# Go to the script directory to build
cd "$(dirname $0)"

## FUNCTIONS ##

function exitTrap() {

    if [[ -n "$PATCH_FILE" && -n "$WORKSPACE" ]]; then
        echo -n "Unpatching edk2..."
        ( cd "$WORKSPACE" && cat Clover/Patches_for_EDK2/$PATCH_FILE | eval "$PATCH_CMD -p0 -R" &>/dev/null )
        if [[ $? -eq 0 ]]; then
            echo " done"
        else
            echo " failed"
        fi
    fi
}

print_option_help () {
  if [[ x$print_option_help_wc = x ]]; then
      if wc -L  </dev/null > /dev/null 2>&1; then
          print_option_help_wc=-L
      elif wc -m  </dev/null > /dev/null 2>&1; then
          print_option_help_wc=-m
      else
          print_option_help_wc=-b
      fi
  fi
  if [[ x$have_fmt = x ]]; then
      if fmt -w 40  </dev/null > /dev/null 2>&1; then
          have_fmt=y;
      else
          have_fmt=n;
      fi
  fi
  local print_option_help_lead="  $1"
  local print_option_help_lspace="$(echo "$print_option_help_lead" | wc $print_option_help_wc)"
  local print_option_help_fill="$((26 - print_option_help_lspace))"
  printf "%s" "$print_option_help_lead"
  local print_option_help_nl=
  if [[ $print_option_help_fill -le 0 ]]; then
      print_option_help_nl=y
      echo
  else
      print_option_help_i=0;
      while [[ $print_option_help_i -lt $print_option_help_fill ]]; do
          printf " "
          print_option_help_i=$((print_option_help_i+1))
      done
      print_option_help_nl=n
  fi
  local print_option_help_split=
  if [[ x$have_fmt = xy ]]; then
      print_option_help_split="$(echo "$2" | fmt -w 50)"
  else
      print_option_help_split="$2"
  fi
  if [[ x$print_option_help_nl = xy ]]; then
      echo "$print_option_help_split" | awk '{ print "                          " $0; }'
  else
      echo "$print_option_help_split" | awk 'BEGIN   { n = 0 }
          { if (n == 1) print "                          " $0; else print $0; n = 1 ; }'
  fi
}

# Function to manage PATH
pathmunge () {
    if [[ ! $PATH =~ (^|:)$1(:|$) ]]; then
        if [[ "${2:-}" = "after" ]]; then
            export PATH=$PATH:$1
        else
            export PATH=$1:$PATH
        fi
    fi
}

# Add edk2 build option
addEdk2BuildOption() {
    EDK2_BUILD_OPTIONS=("${EDK2_BUILD_OPTIONS[@]}" $@)
}

# Add edk2 build macro
addEdk2BuildMacro() {
    local macro="$1"
    addEdk2BuildOption "-D" "$macro"
}

# Print the usage.
usage() {
    echo "Script for building CloverEFI sources on Darwin OS X"
    echo
    printf "Usage: %s [OPTIONS] [all|fds|genc|genmake|clean|cleanpkg|cleanall|cleanlib|modules|libraries]\n" "$SELF"
    echo
    echo "Configuration:"
    print_option_help "-n THREADNUMBER" "Build the platform using multi-threaded compiler [default is number of CPUs + 1]"
    print_option_help "-h, --help"    "print this message and exit"
    print_option_help "-v, --version" "print the version information and exit"
    echo
    echo "Toolchain:"
    print_option_help "--clang"     "use XCode Clang toolchain"
    print_option_help "--gcc"       "use unix GCC toolchain"
    print_option_help "--gcc47"     "use GCC 4.7 toolchain [Default]"
    print_option_help "--xcode"     "use XCode 3.2 toolchain"
    print_option_help "-t TOOLCHAIN, --tagname=TOOLCHAIN" "force to use a specific toolchain"
    echo
    echo "Target:"
    print_option_help "--ia32"      "build Clover in 32-bit [boot3]"
    print_option_help "--x64"       "build Clover in 64-bit [boot6] [Default]"
    print_option_help "--x64-mcp"   "build Clover in 64-bit [boot7] using BiosBlockIO (compatible with MCP chipset)"
    print_option_help "-a TARGETARCH, --arch=TARGETARCH" "overrides target.txt's TARGET_ARCH definition"
    print_option_help "-p PLATFORMFILE, --platform=PLATFORMFILE" "Build the platform specified by the DSC filename argument"
    print_option_help "-b BUILDTARGET, --buildtarget=BUILDTARGET" "using the BUILDTARGET to build the platform"
    echo
    echo "Options:"
    print_option_help "-D MACRO, --define=MACRO" "Macro: \"Name[=Value]\"."
    print_option_help "--vbios-patch-cloverefi" "activate vbios patch in CloverEFI"
    print_option_help "--only-sata0" "activate only SATA0 patch"
    echo
    echo "Report bugs to http://www.projectosx.com/forum/index.php?showtopic=2490"
}

# Manage option argument
argument () {
  local opt=$1
  shift

  if [[ $# -eq 0 ]]; then
      printf "%s: option \`%s' requires an argument\n" "$0" "$opt"
      exit 1
  fi

  echo $1
}

# Check the command line arguments
checkCmdlineArguments() {
    while [[ $# -gt 0 ]]; do
        local option=$1
        shift
        case "$option" in
            -clang  | --clang)   TOOLCHAIN=XCLANG  ;;
            -llvm   | --llvm)    TOOLCHAIN=LLVM  ;;
	    -gcc49  | --gcc49)   TOOLCHAIN=GCC49   ;;
	    -gcc48  | --gcc48)   TOOLCHAIN=GCC48   ;;
            -gcc47  | --gcc47)   TOOLCHAIN=GCC47   ;;
            -unixgcc | --gcc)    TOOLCHAIN=UNIXGCC ;;
            -xcode  | --xcode )  TOOLCHAIN=XCODE32 ;;
            -ia32 | --ia32)      TARGETARCH=IA32   ;;
            -x64 | --x64)        TARGETARCH=X64    ;;
            -mc | --x64-mcp)     TARGETARCH=X64 ; USE_BIOS_BLOCKIO=1 ;;
            -clean)    TARGETRULE=clean ;;
            -cleanall) TARGETRULE=cleanall ;;
            -d | -debug | --debug)  BUILDTARGET=DEBUG ;;
            -r | -release | --release) BUILDTARGET=RELEASE ;;
            -a) TARGETARCH=$(argument $option "$@"); shift
                ;;
            --arch=*)
                TARGETARCH=$(echo "$option" | sed 's/--arch=//')
                ;;
            -p) PLATFORMFILE=$(argument $option "$@"); shift
                ;;
            --platform=*)
                PLATFORMFILE=$(echo "$option" | sed 's/--platform=//')
                ;;
            -b) BUILDTARGET=$(argument $option "$@"); shift
                ;;
            --buildtarget=*)
                BUILDTARGET=$(echo "$option" | sed 's/--buildtarget=//')
                ;;
            -t) TOOLCHAIN=$(argument $option "$@"); shift
                ;;
            --tagname=*)
                TOOLCHAIN=$(echo "$option" | sed 's/--tagname=//')
                ;;
            -D)
                addEdk2BuildMacro $(argument $option "$@"); shift
                ;;
            --define=*)
                addEdk2BuildMacro $(echo "$option" | sed 's/--define=//')
                ;;
            -n)
                BUILDTHREADS=$(argument $option "$@"); shift
                ;;
            --vbios-patch-cloverefi)
                VBIOSPATCHCLOVEREFI=1
                ;;
            --only-sata0)
                ONLYSATA0PATCH=1
                ;;
            -h | -\? | -help | --help)
                usage && exit 0
                ;;
            -v | --version)
                echo "$SELF v1.0" && exit 0
                ;;
            -*)
                printf "Unrecognized option \`%s'\n" "$option" 1>&2
                exit 1
                ;;
            *)
               TARGETRULE="$option"
               ;;
        esac
    done

    # Update variables
    PLATFORMFILE="${PLATFORMFILE:-Shim/Shim.dsc}"
}

## Check tools for the toolchain
checkToolchain() {
    case "$TOOLCHAIN" in
        XCLANG|XCODE32|XCODE41) checkXcode ;;
    esac
}


# Main build script
MainBuildScript() {

    checkCmdlineArguments $@
    checkToolchain

    if [[ -d .git ]]; then
        git svn info | grep Revision | tr -cd [:digit:] >vers.txt
    else
        svnversion -n | tr -d [:alpha:] >vers.txt
    fi

    #
    # Setup workspace if it is not set
    #
    if [[ -z "$WORKSPACE" ]]; then
        echo "Initializing workspace"
        if [[ ! -x "${PWD}"/edksetup.sh ]]; then
            cd ..
        fi

        # This version is for the tools in the BaseTools project.
        # this assumes svn pulls have the same root dir
        #  export EDK_TOOLS_PATH=`pwd`/../BaseTools
        # This version is for the tools source in edk2
        export EDK_TOOLS_PATH="${PWD}"/BaseTools
        source edksetup.sh BaseTools
    else
        echo "Building from: $WORKSPACE"
    fi

    # Trying to patch edk2
    if [[ -n "$PATCH_FILE" ]]; then
        echo -n "Patching edk2..."
        ( cd "$WORKSPACE" && cat Clover/Patches_for_EDK2/$PATCH_FILE | eval "$PATCH_CMD -p0" &>/dev/null )
        if [[ $? -eq 0 ]]; then
            echo " done"
        else
            echo " failed"
        fi
    fi

    export CLOVER_PKG_DIR="$WORKSPACE"/Clover/CloverPackage/CloverV2

    # Cleaning part of the script if we have told to do it
    # Create edk tools if necessary
    if  [[ ! -x "$EDK_TOOLS_PATH/Source/C/bin/GenFv" ]]; then
        echo "Building tools as they are not found"
        make -C "$WORKSPACE"/BaseTools CC="gcc -Wno-deprecated-declarations"
    fi

    # Build Clover
    #rm $WORKSPACE/Clover/Version.h
    local cmd="build ${EDK2_BUILD_OPTIONS[@]}"
    cmd="$cmd -p $PLATFORMFILE -a $TARGETARCH -b $BUILDTARGET"
    cmd="$cmd -t $TOOLCHAIN -n $BUILDTHREADS $TARGETRULE"

    echo
    echo "Running edk2 build for CryptoPkg$TARGETARCH using the command:"
    echo "$cmd"
    echo
    eval "$cmd"
}

# Deploy Clover files for packaging
# BUILD START #
trap 'exitTrap' EXIT

# Default locale
export LC_ALL=POSIX

# Add toolchain bin directory to the PATH
pathmunge "$TOOLCHAIN_DIR/bin"

MainBuildScript $@
# MainPostBuildScript

# Local Variables:      #
# mode: ksh             #
# tab-width: 4          #
# indent-tabs-mode: nil #
# End:                  #
#
# vi: set expandtab ts=4 sw=4 sts=4: #
