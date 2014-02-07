#!/bin/bash

set -e

source ./scripts/mod_helpers.sh

if test "$(mod_filename mac80211)" = "mac80211.ko.gz" ; then
	compr=".gz"
else
	compr=""
fi

for driver in $(find ${BACKPORT_PWD} -type f -name *.ko); do
	mod_name=${driver/${BACKPORT_PWD}/${KLIB}${KMODDIR}}${compr}
	echo "  uninstall" $mod_name
	rm -f $mod_name
done
