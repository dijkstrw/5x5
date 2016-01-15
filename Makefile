BINARY = 5x5
OBJS = clock.o usb.o serial.o ring.o debug.o matrix.o keymap.o keyboard.o mouse.o extrakey.o elog.o led.o

OPENCM3_DIR = libopencm3
STLINK_PORT = 4242
OPENOCD_PORT = 3333
LDSCRIPT = stm32f103x8.ld
include libopencm3.stm32f1.mk

flash: libopencm3 flash_stlink

debug: debug_stlink

libopencm3:
	git submodule init
	git submodule update

size: $(BINARY).elf
	$(Q)./checksize $(LDSCRIPT) $(BINARY).elf

flash_stlink: $(BINARY).bin
	st-flash write $(BINARY).bin 0x8000000

.gdb_config_stlink:
	echo -e > .gdb_config_stlink "file $(BINARY).elf\ntarget remote :$(STLINK_PORT)\nbreak main\n"

debug_stlink: .gdb_config_stlink
	st-util -p $(STLINK_PORT) &
	$(GDB) --command=.gdb_config_stlink
	pkill -9 st-util

flash_oocd: $(BINARY).hex
	openocd -f openocd/interface/jlink.cfg \
		-f openocd/target/stm32f1x.cfg \
		-c "init" -c "reset init" \
		-c "flash write_image erase $(BINARY).hex" \
		-c "reset" \
		-c "shutdown" $(NULL)

.gdb_config_oocd:
	echo -e > .gdb_config_oocd "file $(BINARY).elf\ntarget remote :$(OPENOCD_PORT)\nbreak main\n"

debug_oocd: .gdb_config_oocd
	openocd -f openocd/interface/jlink.cfg \
		-f openocd/target/stm32f1x.cfg &
	$(GDB) --command=.gdb_config_oocd
	pkill -9 openocd
