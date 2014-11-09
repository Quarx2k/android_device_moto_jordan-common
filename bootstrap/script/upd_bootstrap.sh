#!/sbin/sh

cp -R /system/bootstrap/ /bootstrap/
cp -R /system/bootstrap/binary/logwrapper /bootstrap/bin/logwrapper
chmod 777 /bootstrap/bin/logwrapper
chmod 777 /bootstrap/bootstrap/script/*
chmod 777 /bootstrap/bootstrap/binary/*
chmod 777 /bootstrap/bootstrap/2nd-boot/*

