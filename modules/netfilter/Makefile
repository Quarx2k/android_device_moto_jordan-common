# quota2
obj-m  += xt_quota2.o

# qtaguid for ICS usage stats
ccflags-y += -UCONFIG_GENERIC_ATOMIC64
ccflags-y += -UDDEBUG

obj-m  += xt_owner2.o

xt_owner2-objs := xt_qtaguid.o xt_qtaguid_print.o xt_qtaguid_sockets.o
