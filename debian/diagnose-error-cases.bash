#!/bin/bash

distro="$(lsb_release --id --short)"
if [ "$distro" != "Debian" -a "$distro" != "Ubuntu" ]; then
    echo Unsupported distro $distro
    exit 1
fi

# 2.6.32-5-amd64
# 2.6.32-37-generic
abiname="$(cut -d " " -f 3 /proc/version)"

# 2.6.32
baseversion="$(echo "$abiname" | cut -d "-" -f 1)"

case "$distro" in 
Debian) # 2.6.32-39
	if uname -v | grep -q Debian; then
	    version=$(uname -v | cut -d " " -f 4)
	else
	    version="$(cut -d " " -f 5 /proc/version | cut -d ")" -f 1)"
	fi
	;;
Ubuntu)
	# 2.6.32-37.81
	version="$(cut -d " " -f 2 /proc/version_signature | cut -d "-" -f 1-2)"
	;;
esac


(
echo make >= 0
echo linux-image-$abiname = $version
echo linux-headers-$abiname = $version
echo linux-kbuild-$baseversion >= $version
case "$distro" in
Debian) echo linux-image-$abiname-dbg = $version
	;;
Ubuntu) echo linux-image-$abiname-dbgsym = $version
	;;
esac
) | while read package relation requiredversion; do
    installedversion="$(dpkg-query -W "$package" 2> /dev/null | cut -f 2)"
    if [ "$installedversion" = "" ]; then
	availableversion="$(apt-cache show $package 2> /dev/null | grep ^Version: | cut -d " " -f 2)"
	if [ "$availableversion" = "" ]; then
	    echo "You need package $package but it does not seem to be available"
	    if [ "$distro" = "Ubuntu" -a "$(echo $package | grep dbgsym$)" ]; then
		echo " Ubuntu -dbgsym packages are typically in a separate repository"
		echo " Follow https://wiki.edubuntu.org/DebuggingProgramCrash to add this repository"
	    elif [ "$distro" = "Debian" -a "$(echo $package | grep dbg$)" ]; then
		echo " Debian does not have -dbg packages for all kernels. Consider switching to a kernel that has one."
	    fi
	else
	    echo "Please install $package"
	fi
    elif ! dpkg --compare-versions $installedversion $relation $requiredversion; then
	echo "Package $package version $installedversion does not match version of currently running kernel: $requiredversion"
	echo " Consider apt-get upgrade && reboot"
    fi
done

user="$(id --user --name)"
if [ "$user" != "root" ]; then
    groups="$(id --groups --name)"
    for i in stapusr stapdev; do
	if [ "$(echo $groups | grep $i)" = "" ]; then
	    echo "Be root or adduser $user $i"
	fi
    done
fi
