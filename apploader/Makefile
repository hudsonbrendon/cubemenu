# Built inside the libogc2 container: make -C apploader
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy
apploader.bin: apploader.c apploader.ld
	$(CC) -O2 -mcpu=750 -meabi -mhard-float -ffreestanding -fno-builtin -nostdlib -G 0 -ffunction-sections \
	  -Wall -Wl,-T,apploader.ld -Wl,--gc-sections -Wl,-Map,apploader.map -o apploader.elf apploader.c
	$(OBJCOPY) -O binary apploader.elf apploader.bin
	@$(DEVKITPPC)/bin/powerpc-eabi-nm apploader.elf | grep ap_entry
clean:
	rm -f apploader.elf apploader.bin apploader.map
