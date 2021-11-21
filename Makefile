BINARY = 5x5
OBJS = automouse.o clock.o command.o debug.o elog.o extrakey.o		\
       keyboard.o keymap.o led.o macro.o matrix.o mouse.o map_ascii.o	\
       ring.o serial.o usb.o

GOJIRA_VERSION = $(shell git describe --tags --always)
CFLAGS = -DGOJIRA_VERSION='"$(GOJIRA_VERSION)"'
OPENCM3_DIR = libopencm3
STLINK_PORT = 4242
OPENOCD_PORT = 3333
BMP_PORT = /dev/ttyACM0
LDSCRIPT = stm32f103x8.ld
include libopencm3.stm32f1.mk

flash: flash_stlink

size: $(BINARY).elf
	$(Q)./checksize $(LDSCRIPT) $(BINARY).elf

flash_stlink: $(BINARY).bin
	st-flash write $(BINARY).bin 0x8000000

flash_oocd: $(BINARY).hex
	openocd -f interface/stlink-v2.cfg \
		-f target/stm32f1x.cfg \
		-c "init" -c "reset init" \
		-c "flash write_image erase $(BINARY).hex" \
		-c "reset" \
		-c "shutdown" $(NULL)

debug: debug_oocd

.gdb_config_oocd:
	echo -e > .gdb_config_oocd "file $(BINARY).elf\ntarget remote :$(OPENOCD_PORT)\nbreak main\n"

debug_oocd: .gdb_config_oocd
	openocd -f interface/stlink-v2-1.cfg \
		-f target/stm32f1x_stlink.cfg &
	$(GDB) --command=.gdb_config_oocd
	pkill -9 openocd

.gdb_config_bmp:
	echo > .gdb_config "file $(BINARY).elf\ntarget extended-remote $(BMP_PORT)\nmonitor version\nmonitor swdp_scan\nattach 1\nbreak main\nset mem inaccessible-by-default off\n"

debug_bmp: .gdb_config_bmp
	$(GDB) --command=.gdb_config
