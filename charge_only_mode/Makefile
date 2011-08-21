<<<<<<< HEAD
all: exe copy

exe:
	cd ~/a ; make -j8 out/target/product/morrison/obj/EXECUTABLES/charge_only_mode_intermediates/charge_only_mode ; cd -

copy:
	adb push ~/a/out/target/product/morrison/obj/EXECUTABLES/charge_only_mode_intermediates/charge_only_mode data/com
=======
TOP := ../../../..
PRODUCT := jordan

all: exe

exe:
	make -j8 ${TOP}/out/target/product/${PRODUCT}/obj/EXECUTABLES/charge_only_mode_intermediates/charge_only_mode ; cd -

copy:
	adb push ${TOP}/out/target/product/${PRODUCT}/obj/EXECUTABLES/charge_only_mode_intermediates/charge_only_mode /system/bin/
>>>>>>> cdefy/gingerbread
