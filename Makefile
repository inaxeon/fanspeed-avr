##############################################################################
# Title        : AVR Makefile for Windows
#
# Created      : Matthew Millman 2019-11-26
#                http://tech.mattmillman.com/
#
##############################################################################

# Fixes clash between windows and coreutils mkdir. Comment out the below line to compile on Linux
COREUTILS  = C:/Projects/coreutils/bin/

DEVICE     = atmega328
PROGRAMMER = -c arduino -P COM3 -c stk500 -b 115200 
SRCS       = main.c timer.c ds2482.c ow_bitbang.c ds18x20.c config.c util.c usart_buffered.c i2c.c pwm.c crc8.c
OBJS       = $(SRCS:.c=.o)
FUSES      = -U lfuse:w:0xDC:m -U hfuse:w:0xD1:m -U efuse:w:0xFC:m
DEPDIR     = deps
DEPFLAGS   = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
RM         = rm
MV         = mv
MKDIR      = $(COREUTILS)mkdir

POSTCOMPILE = $(MV) $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE) -V
COMPILE = avr-gcc -Wall -Os $(DEPFLAGS) -mmcu=$(DEVICE)

all:	fanspeed.hex

.c.o:
	@$(MKDIR) -p $(DEPDIR)
	$(COMPILE) -c $< -o $@
	@$(POSTCOMPILE)

.S.o:
	@$(MKDIR) -p $(DEPDIR)
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
	@$(POSTCOMPILE)

.c.s:
	@$(MKDIR) -p $(DEPDIR)
	$(COMPILE) -S $< -o $@
	@$(POSTCOMPILE)

flash:	all
	$(AVRDUDE) -U flash:w:fanspeed.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

install: flash

clean:
	$(RM) -rf deps fanspeed.hex fanspeed.elf $(OBJS)

fanspeed.elf: $(OBJS)
	$(COMPILE) -o fanspeed.elf $(OBJS)

fanspeed.hex: fanspeed.elf
	avr-objcopy -j .text -j .data -O ihex fanspeed.elf fanspeed.hex

disasm:	fanspeed.elf
	avr-objdump -d fanspeed.elf

cpp:
	$(COMPILE) -E $(SRCS)

$(DEPDIR)/%.d:
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))