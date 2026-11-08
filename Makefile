
# Visor de Miis - Nintendo Wii
# devkitPPC + libogc

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO no esta definido)
endif

ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC no esta definido)
endif

include $(DEVKITPPC)/wii_rules

TARGET := boot
BUILD  := build
SOURCE := source

LIBOGC_INC := $(DEVKITPRO)/libogc/include
LIBOGC_LIB := $(DEVKITPRO)/libogc/lib/wii

CFLAGS := -O2 -Wall -Wextra
CFLAGS += -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float
CFLAGS += -I$(SOURCE) -I$(LIBOGC_INC)

LDFLAGS := -g

LIBS := -L$(LIBOGC_LIB) -lfat -lwiiuse -lbte -logc -lm

SOURCES := $(wildcard $(SOURCE)/*.c)
OBJECTS := $(patsubst $(SOURCE)/%.c,$(BUILD)/%.o,$(SOURCES))
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET).dol

$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(MACHDEP) $(OBJECTS) $(LIBS) -o $@

$(TARGET).dol: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2dol $< $@

$(BUILD)/%.o: $(SOURCE)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol

-include $(DEPENDS)
