#!/sbin/sh

cp -R /system/bootstrap/* /bootstrap/bootstrap/*
cp -R /system/bootstrap/bootstrap/binary/logwrapper /bootstrap/bin/logwrapper
chmod 777 /bootstrap/bin/logwrapper
chmod 777 /bootstrap/bootstrap/scripts/*

