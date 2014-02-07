CFLAGS=-Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer

LXDIALOG := lxdialog/checklist.o lxdialog/inputbox.o lxdialog/menubox.o lxdialog/textbox.o lxdialog/util.o lxdialog/yesno.o

conf: conf.o zconf.tab.o
mconf: LDFLAGS = -Wl,--no-as-needed $(shell ./lxdialog/check-lxdialog.sh -ldflags $(CC))
mconf: CFLAGS += -DCURSES_LOC="<ncurses.h>" -DLOCALE
mconf: mconf.o zconf.tab.o $(LXDIALOG)

.PHONY: clean
clean:
	@rm -f mconf conf *.o lxdialog/*.o
