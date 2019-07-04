rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

TARGET = controller

BUILD_DIR = .build

DEBUG = 0

#OPT = -Og
OPT = -Os


C_SOURCES = $(call rwildcard,src/,*.c) $(call rwildcard,lib/CubeMX/,*.c)

CXX_SOURCES = $(call rwildcard, src/,*.cpp)

ASM_SOURCES = lib/CubeMX/startup_stm32f446xx.s

PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
AR = $(PREFIX)ar
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

AS_DEFS =
AS_INCLUDES =

CPP_DEFS = -DUSE_HAL_DRIVER -DSTM32F446xx
CPP_INCLUDES =  \
-Iinclude \
-Ilib/CubeMX/USB_HOST/App \
-Ilib/CubeMX/USB_HOST/Target \
-Ilib/CubeMX/Core/Inc \
-Ilib/CubeMX/Drivers/STM32F4xx_HAL_Driver/Inc \
-Ilib/CubeMX/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
-Ilib/CubeMX/Middlewares/ST/STM32_USB_Host_Library/Core/Inc \
-Ilib/CubeMX/Middlewares/ST/STM32_USB_Host_Library/Class/HID/Inc \
-Ilib/CubeMX/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
-Ilib/CubeMX/Drivers/CMSIS/Include

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CPPFLAGS = $(MCU) $(CPP_DEFS) $(CPP_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CXXFLAGS = -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(DEBUG), 1)
CPPFLAGS += -g -gdwarf-2
endif

CPPFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)"

LDSCRIPT = lib/CubeMX/STM32F446RETx_FLASH.ld

LIBS = -lc -lgcc -lm -lstdc++ -lnosys
LIBDIR = 
LDFLAGS = $(MCU) --specs=nano.specs --specs=nosys.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) $(OPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections,--relax

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
#all:
#	@echo "$(C_SOURCES)"

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(CXX_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CPPFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR) 
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cpp=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

clean:
	-rm -rf .dep $(BUILD_DIR)

-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)
