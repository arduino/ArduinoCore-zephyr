#define CONFIG_ADC_INIT_PRIORITY 50
#define CONFIG_SERIAL 1
#define CONFIG_UART_INTERRUPT_DRIVEN 1
#define CONFIG_I2C 1
#define CONFIG_SPI 1
#define CONFIG_NUM_IRQS 150
#define CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC 480000000
#define CONFIG_FLASH_SIZE 1024
#define CONFIG_FLASH_BASE_ADDRESS 0x8000000
#define CONFIG_MP_MAX_NUM_CPUS 1
#define CONFIG_SYS_CLOCK_TICKS_PER_SEC 10000
#define CONFIG_ROM_START_OFFSET 0x0
#define CONFIG_PINCTRL 1
#define CONFIG_BUILD_OUTPUT_BIN 1
#define CONFIG_XIP 1
#define CONFIG_MAIN_STACK_SIZE 32768
#define CONFIG_IDLE_STACK_SIZE 320
#define CONFIG_HAS_FLASH_LOAD_OFFSET 1
#define CONFIG_CPU_HAS_ARM_MPU 1
#define CONFIG_CPU_HAS_FPU_DOUBLE_PRECISION 1
#define CONFIG_STM32H7_DUAL_CORE 1
#define CONFIG_TICKLESS_KERNEL 1
#define CONFIG_FPU 1
#define CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE 1024
#define CONFIG_CORTEX_M_SYSTICK 1
#define CONFIG_CLOCK_CONTROL_STM32_CUBE 1
#define CONFIG_CLOCK_CONTROL_INIT_PRIORITY 1
#define CONFIG_USE_DT_CODE_PARTITION 1
#define CONFIG_CACHE_MANAGEMENT 1
#define CONFIG_FLASH_LOAD_OFFSET 0x40000
#define CONFIG_FLASH 1
#define CONFIG_HEAP_MEM_POOL_SIZE 32768
#define CONFIG_SERIAL_INIT_PRIORITY 50
#define CONFIG_FLASH_FILL_BUFFER_SIZE 32
#define CONFIG_GPIO 1
#define CONFIG_CPU_HAS_CUSTOM_FIXED_SOC_MPU_REGIONS 1
#define CONFIG_KERNEL_ENTRY "__start"
#define CONFIG_CACHE 1
#define CONFIG_DCACHE 1
#define CONFIG_LOG 1
#define CONFIG_DCACHE_LINE_SIZE 32
#define CONFIG_GEN_IRQ_VECTOR_TABLE 1
#define CONFIG_SOC "stm32h747xx"
#define CONFIG_RESET 1
#define CONFIG_ARCH_SW_ISR_TABLE_ALIGN 4
#define CONFIG_LOG_DOMAIN_NAME ""
#define CONFIG_BUILD_OUTPUT_HEX 1
#define CONFIG_CLOCK_CONTROL 1
#define CONFIG_UART_USE_RUNTIME_CONFIGURE 1
#define CONFIG_SOC_SERIES "stm32h7x"
#define CONFIG_SOC_FAMILY "stm32"
#define CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE -1
#define CONFIG_GEN_ISR_TABLES 1
#define CONFIG_GEN_SW_ISR_TABLE 1
#define CONFIG_REBOOT 1
#define CONFIG_FLASH_INIT_PRIORITY 50
#define CONFIG_GEN_IRQ_START_VECTOR 0
#define CONFIG_SRAM_OFFSET 0x0
#define CONFIG_CONSOLE 1
#define CONFIG_ARCH_IRQ_VECTOR_TABLE_ALIGN 4
#define CONFIG_ISR_STACK_SIZE 2048
#define CONFIG_ICACHE_LINE_SIZE 32
#define CONFIG_DT_HAS_ARDUINO_HEADER_R3_ENABLED 1
#define CONFIG_DT_HAS_ARM_ARMV7M_MPU_ENABLED 1
#define CONFIG_DT_HAS_ARM_ARMV7M_SYSTICK_ENABLED 1
#define CONFIG_DT_HAS_ARM_CORTEX_M7_ENABLED 1
#define CONFIG_DT_HAS_ARM_V7M_NVIC_ENABLED 1
#define CONFIG_DT_HAS_FIXED_CLOCK_ENABLED 1
#define CONFIG_DT_HAS_FIXED_PARTITIONS_ENABLED 1
#define CONFIG_DT_HAS_GPIO_KEYS_ENABLED 1
#define CONFIG_DT_HAS_GPIO_LEDS_ENABLED 1
#define CONFIG_DT_HAS_INFINEON_CYW43XXX_BT_HCI_ENABLED 1
#define CONFIG_DT_HAS_MMIO_SRAM_ENABLED 1
#define CONFIG_DT_HAS_SOC_NV_FLASH_ENABLED 1
#define CONFIG_DT_HAS_ST_MBOX_STM32_HSEM_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_ADC_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_DAC_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_EXTI_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_FLASH_CONTROLLER_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_FMC_SDRAM_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_GPIO_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_HSE_CLOCK_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_HSEM_MAILBOX_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_I2C_V2_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_LSE_CLOCK_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_NV_FLASH_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_OTGFS_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_PINCTRL_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_QSPI_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_QSPI_NOR_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_RCC_RCTL_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_RNG_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_SPI_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_SPI_FIFO_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_UART_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32_USART_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32H7_FDCAN_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32H7_FLASH_CONTROLLER_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32H7_FMC_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32H7_PLL_CLOCK_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32H7_RCC_ENABLED 1
#define CONFIG_DT_HAS_ST_STM32H7_SPI_ENABLED 1
#define CONFIG_DT_HAS_USB_NOP_XCEIV_ENABLED 1
#define CONFIG_DT_HAS_ZEPHYR_BT_HCI_UART_ENABLED 1
#define CONFIG_DT_HAS_ZEPHYR_CDC_ACM_UART_ENABLED 1
#define CONFIG_DT_HAS_ZEPHYR_MEMORY_REGION_ENABLED 1
#define CONFIG_ZEPHYR_ARDUINO_API_MODULE 1
#define CONFIG_ZEPHYR_CMSIS_MODULE 1
#define CONFIG_HAS_CMSIS_CORE 1
#define CONFIG_HAS_CMSIS_CORE_M 1
#define CONFIG_ZEPHYR_HAL_AMBIQ_MODULE 1
#define CONFIG_ZEPHYR_HAL_ESPRESSIF_MODULE 1
#define CONFIG_ZEPHYR_HAL_INFINEON_MODULE 1
#define CONFIG_ZEPHYR_HAL_INTEL_MODULE 1
#define CONFIG_ZEPHYR_HAL_NORDIC_MODULE 1
#define CONFIG_ZEPHYR_HAL_NXP_MODULE 1
#define CONFIG_ZEPHYR_HAL_RENESAS_MODULE 1
#define CONFIG_ZEPHYR_STM32_MODULE 1
#define CONFIG_ZEPHYR_TI_MODULE 1
#define CONFIG_ZEPHYR_XTENSA_MODULE 1
#define CONFIG_ZEPHYR_LITTLEFS_MODULE 1
#define CONFIG_ZEPHYR_MBEDTLS_MODULE 1
#define CONFIG_ZEPHYR_SEGGER_MODULE 1
#define CONFIG_HAS_SEGGER_RTT 1
#define CONFIG_ZEPHYR_TINYCRYPT_MODULE 1
#define CONFIG_HAS_STM32CUBE 1
#define CONFIG_USE_STM32_HAL_CORTEX 1
#define CONFIG_USE_STM32_HAL_MDMA 1
#define CONFIG_USE_STM32_HAL_PCD 1
#define CONFIG_USE_STM32_HAL_PCD_EX 1
#define CONFIG_USE_STM32_HAL_QSPI 1
#define CONFIG_USE_STM32_LL_I2C 1
#define CONFIG_USE_STM32_LL_RCC 1
#define CONFIG_USE_STM32_LL_SPI 1
#define CONFIG_USE_STM32_LL_USB 1
#define CONFIG_USE_STM32_LL_UTILS 1
#define CONFIG_BOARD "arduino_giga_r1"
#define CONFIG_BOARD_REVISION ""
#define CONFIG_BOARD_TARGET "arduino_giga_r1/stm32h747xx/m7"
#define CONFIG_BOARD_ARDUINO_GIGA_R1 1
#define CONFIG_BOARD_ARDUINO_GIGA_R1_STM32H747XX_M7 1
#define CONFIG_BOARD_QUALIFIERS "stm32h747xx/m7"
#define CONFIG_SOC_FAMILY_STM32 1
#define CONFIG_SOC_SERIES_STM32H7X 1
#define CONFIG_SOC_STM32H747XX_M7 1
#define CONFIG_STM32H7_BOOT_M4_AT_INIT 1
#define CONFIG_SWJ_ANALOG_PRIORITY 49
#define CONFIG_POWER_SUPPLY_LDO 1
#define CONFIG_GPIO_INIT_PRIORITY 40
#define CONFIG_SOC_LOG_LEVEL_DEFAULT 1
#define CONFIG_SOC_LOG_LEVEL 3
#define CONFIG_ARCH "arm"
#define CONFIG_ARCH_HAS_SINGLE_THREAD_SUPPORT 1
#define CONFIG_CPU_CORTEX 1
#define CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE 4
#define CONFIG_CPU_CORTEX_M 1
#define CONFIG_ISA_THUMB2 1
#define CONFIG_ASSEMBLER_ISA_THUMB2 1
#define CONFIG_COMPILER_ISA_THUMB2 1
#define CONFIG_STACK_ALIGN_DOUBLE_WORD 1
#define CONFIG_RUNTIME_NMI 1
#define CONFIG_FAULT_DUMP 2
#define CONFIG_ARM_STORE_EXC_RETURN 1
#define CONFIG_FP_HARDABI 1
#define CONFIG_FP16 1
#define CONFIG_FP16_IEEE 1
#define CONFIG_CPU_CORTEX_M7 1
#define CONFIG_CPU_CORTEX_M_HAS_SYSTICK 1
#define CONFIG_CPU_CORTEX_M_HAS_DWT 1
#define CONFIG_CPU_CORTEX_M_HAS_BASEPRI 1
#define CONFIG_CPU_CORTEX_M_HAS_VTOR 1
#define CONFIG_CPU_CORTEX_M_HAS_PROGRAMMABLE_FAULT_PRIOS 1
#define CONFIG_ARMV7_M_ARMV8_M_MAINLINE 1
#define CONFIG_ARMV7_M_ARMV8_M_FP 1
#define CONFIG_NULL_POINTER_EXCEPTION_DETECTION_NONE 1
#define CONFIG_HAS_SWO 1
#define CONFIG_ARM 1
#define CONFIG_ARCH_IS_SET 1
#define CONFIG_ARCH_LOG_LEVEL_DEFAULT 1
#define CONFIG_ARCH_LOG_LEVEL 3
#define CONFIG_LITTLE_ENDIAN 1
#define CONFIG_SRAM_SIZE 512
#define CONFIG_SRAM_BASE_ADDRESS 0x24000000
#define CONFIG_GEN_PRIV_STACKS 1
#define CONFIG_ISR_TABLES_LOCAL_DECLARATION_SUPPORTED 1
#define CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_ADDRESS 1
#define CONFIG_EXCEPTION_DEBUG 1
#define CONFIG_ARCH_HAS_TIMING_FUNCTIONS 1
#define CONFIG_ARCH_HAS_RAMFUNC_SUPPORT 1
#define CONFIG_ARCH_HAS_NESTED_EXCEPTION_DETECTION 1
#define CONFIG_ARCH_SUPPORTS_COREDUMP 1
#define CONFIG_ARCH_SUPPORTS_ARCH_HW_INIT 1
#define CONFIG_ARCH_SUPPORTS_ROM_START 1
#define CONFIG_ARCH_HAS_EXTRA_EXCEPTION_INFO 1
#define CONFIG_ARCH_HAS_THREAD_LOCAL_STORAGE 1
#define CONFIG_ARCH_HAS_SUSPEND_TO_RAM 1
#define CONFIG_ARCH_HAS_THREAD_ABORT 1
#define CONFIG_ARCH_HAS_CODE_DATA_RELOCATION 1
#define CONFIG_CPU_HAS_FPU 1
#define CONFIG_CPU_HAS_MPU 1
#define CONFIG_CPU_HAS_DCACHE 1
#define CONFIG_CPU_HAS_ICACHE 1
#define CONFIG_FPU_SHARING 1
#define CONFIG_ICACHE 1
#define CONFIG_ARCH_CACHE 1
#define CONFIG_TOOLCHAIN_HAS_BUILTIN_FFS 1
#define CONFIG_KERNEL_LOG_LEVEL_DEFAULT 1
#define CONFIG_KERNEL_LOG_LEVEL 3
#define CONFIG_MULTITHREADING 1
#define CONFIG_NUM_COOP_PRIORITIES 16
#define CONFIG_NUM_PREEMPT_PRIORITIES 15
#define CONFIG_MAIN_THREAD_PRIORITY 0
#define CONFIG_COOP_ENABLED 1
#define CONFIG_PREEMPT_ENABLED 1
#define CONFIG_PRIORITY_CEILING -127
#define CONFIG_NUM_METAIRQ_PRIORITIES 0
#define CONFIG_THREAD_STACK_INFO 1
#define CONFIG_SCHED_DUMB 1
#define CONFIG_WAITQ_DUMB 1
#define CONFIG_LIBC_ERRNO 1
#define CONFIG_ERRNO 1
#define CONFIG_CURRENT_THREAD_USE_TLS 1
#define CONFIG_INIT_STACKS 1
#define CONFIG_BOOT_BANNER 1
#define CONFIG_BOOT_BANNER_STRING "Booting Zephyr OS build"
#define CONFIG_BOOT_DELAY 0
#define CONFIG_THREAD_MONITOR 1
#define CONFIG_THREAD_NAME 1
#define CONFIG_THREAD_MAX_NAME_LEN 32
#define CONFIG_SYSTEM_WORKQUEUE_PRIORITY -1
#define CONFIG_BARRIER_OPERATIONS_ARCH 1
#define CONFIG_ATOMIC_OPERATIONS_BUILTIN 1
#define CONFIG_TIMESLICING 1
#define CONFIG_TIMESLICE_SIZE 0
#define CONFIG_TIMESLICE_PRIORITY 0
#define CONFIG_POLL 1
#define CONFIG_NUM_MBOX_ASYNC_MSGS 10
#define CONFIG_KERNEL_MEM_POOL 1
#define CONFIG_ARCH_HAS_CUSTOM_SWAP_TO_MAIN 1
#define CONFIG_SWAP_NONATOMIC 1
#define CONFIG_SYS_CLOCK_EXISTS 1
#define CONFIG_TIMEOUT_64BIT 1
#define CONFIG_SYS_CLOCK_MAX_TIMEOUT_DAYS 365
#define CONFIG_MP_NUM_CPUS 1
#define CONFIG_TOOLCHAIN_SUPPORTS_THREAD_LOCAL_STORAGE 1
#define CONFIG_THREAD_LOCAL_STORAGE 1
#define CONFIG_KERNEL_WHOLE_ARCHIVE 1
#define CONFIG_TOOLCHAIN_SUPPORTS_STATIC_INIT_GNU 1
#define CONFIG_KERNEL_INIT_PRIORITY_OBJECTS 30
#define CONFIG_KERNEL_INIT_PRIORITY_LIBC 35
#define CONFIG_KERNEL_INIT_PRIORITY_DEFAULT 40
#define CONFIG_KERNEL_INIT_PRIORITY_DEVICE 50
#define CONFIG_APPLICATION_INIT_PRIORITY 90
#define CONFIG_ADC 1
#define CONFIG_ADC_LOG_LEVEL_DEFAULT 1
#define CONFIG_ADC_LOG_LEVEL 3
#define CONFIG_ADC_STM32 1
#define CONFIG_CACHE_LOG_LEVEL_DEFAULT 1
#define CONFIG_CACHE_LOG_LEVEL 3
#define CONFIG_CLOCK_CONTROL_LOG_LEVEL_DEFAULT 1
#define CONFIG_CLOCK_CONTROL_LOG_LEVEL 3
#define CONFIG_CLOCK_STM32_HSE_CLOCK 16000000
#define CONFIG_CLOCK_STM32_MCO1_SRC_NOCLOCK 1
#define CONFIG_CLOCK_STM32_MCO2_SRC_NOCLOCK 1
#define CONFIG_CONSOLE_INPUT_MAX_LINE_LEN 128
#define CONFIG_CONSOLE_HAS_DRIVER 1
#define CONFIG_CONSOLE_INIT_PRIORITY 60
#define CONFIG_UART_CONSOLE 1
#define CONFIG_UART_CONSOLE_LOG_LEVEL_DEFAULT 1
#define CONFIG_UART_CONSOLE_LOG_LEVEL 3
#define CONFIG_FLASH_HAS_DRIVER_ENABLED 1
#define CONFIG_FLASH_HAS_EXPLICIT_ERASE 1
#define CONFIG_FLASH_HAS_PAGE_LAYOUT 1
#define CONFIG_FLASH_JESD216 1
#define CONFIG_FLASH_LOG_LEVEL_DEFAULT 1
#define CONFIG_FLASH_LOG_LEVEL 3
#define CONFIG_FLASH_PAGE_LAYOUT 1
#define CONFIG_SOC_FLASH_STM32 1
#define CONFIG_FLASH_STM32_QSPI 1
#define CONFIG_GPIO_LOG_LEVEL_DEFAULT 1
#define CONFIG_GPIO_LOG_LEVEL 3
#define CONFIG_GPIO_STM32 1
#define CONFIG_HWINFO 1
#define CONFIG_HWINFO_LOG_LEVEL_DEFAULT 1
#define CONFIG_HWINFO_LOG_LEVEL 3
#define CONFIG_HWINFO_STM32 1
#define CONFIG_I2C_STM32 1
#define CONFIG_I2C_STM32_V2 1
#define CONFIG_I2C_STM32_INTERRUPT 1
#define CONFIG_I2C_INIT_PRIORITY 50
#define CONFIG_I2C_LOG_LEVEL_DEFAULT 1
#define CONFIG_I2C_LOG_LEVEL 3
#define CONFIG_INTC_INIT_PRIORITY 40
#define CONFIG_INTC_LOG_LEVEL_DEFAULT 1
#define CONFIG_INTC_LOG_LEVEL 3
#define CONFIG_EXTI_STM32 1
#define CONFIG_PINCTRL_LOG_LEVEL_DEFAULT 1
#define CONFIG_PINCTRL_LOG_LEVEL 3
#define CONFIG_PINCTRL_STM32 1
#define CONFIG_PINCTRL_STM32_REMAP_INIT_PRIORITY 2
#define CONFIG_PWM 1
#define CONFIG_PWM_LOG_LEVEL_DEFAULT 1
#define CONFIG_PWM_LOG_LEVEL 3
#define CONFIG_PWM_INIT_PRIORITY 50
#define CONFIG_RESET_INIT_PRIORITY 35
#define CONFIG_RESET_STM32 1
#define CONFIG_SERIAL_HAS_DRIVER 1
#define CONFIG_SERIAL_SUPPORT_INTERRUPT 1
#define CONFIG_UART_LOG_LEVEL_DEFAULT 1
#define CONFIG_UART_LOG_LEVEL 3
#define CONFIG_UART_LINE_CTRL 1
#define CONFIG_UART_STM32 1
#define CONFIG_SPI_INIT_PRIORITY 50
#define CONFIG_SPI_COMPLETION_TIMEOUT_TOLERANCE 200
#define CONFIG_SPI_LOG_LEVEL_DEFAULT 1
#define CONFIG_SPI_LOG_LEVEL 3
#define CONFIG_SPI_STM32 1
#define CONFIG_SPI_STM32_USE_HW_SS 1
#define CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER 1
#define CONFIG_SYSTEM_CLOCK_INIT_PRIORITY 0
#define CONFIG_TICKLESS_CAPABLE 1
#define CONFIG_SYSTEM_TIMER_HAS_DISABLE_SUPPORT 1
#define CONFIG_CORTEX_M_SYSTICK_INSTALL_ISR 1
#define CONFIG_CORTEX_M_SYSTICK_64BIT_CYCLE_COUNTER 1
#define CONFIG_USB_DEVICE_DRIVER 1
#define CONFIG_USB_DEVICE_REMOTE_WAKEUP 1
#define CONFIG_USB_DC_STM32 1
#define CONFIG_USB_DC_STM32_CLOCK_CHECK 1
#define CONFIG_USB_DRIVER_LOG_LEVEL_DEFAULT 1
#define CONFIG_USB_DRIVER_LOG_LEVEL 3
#define CONFIG_NRF_USBD_COMMON_LOG_LEVEL_DEFAULT 1
#define CONFIG_NRF_USBD_COMMON_LOG_LEVEL 3
#define CONFIG_USBC_LOG_LEVEL_DEFAULT 1
#define CONFIG_USBC_LOG_LEVEL 3
#define CONFIG_FULL_LIBC_SUPPORTED 1
#define CONFIG_MINIMAL_LIBC_SUPPORTED 1
#define CONFIG_NEWLIB_LIBC_SUPPORTED 1
#define CONFIG_PICOLIBC_SUPPORTED 1
#define CONFIG_PICOLIBC 1
#define CONFIG_HAS_NEWLIB_LIBC_NANO 1
#define CONFIG_COMMON_LIBC_ABORT 1
#define CONFIG_COMMON_LIBC_MALLOC 1
#define CONFIG_COMMON_LIBC_CALLOC 1
#define CONFIG_COMMON_LIBC_REALLOCARRAY 1
#define CONFIG_PICOLIBC_USE_TOOLCHAIN 1
#define CONFIG_PICOLIBC_IO_FLOAT 1
#define CONFIG_STDOUT_CONSOLE 1
#define CONFIG_NEED_LIBC_MEM_PARTITION 1
#define CONFIG_SYS_HEAP_ALLOC_LOOPS 3
#define CONFIG_SYS_HEAP_AUTO 1
#define CONFIG_ZVFS_OPEN_MAX 4
#define CONFIG_MPSC_PBUF 1
#define CONFIG_CBPRINTF_COMPLETE 1
#define CONFIG_CBPRINTF_FULL_INTEGRAL 1
#define CONFIG_CBPRINTF_FP_SUPPORT 1
#define CONFIG_CBPRINTF_PACKAGE_LOG_LEVEL_DEFAULT 1
#define CONFIG_CBPRINTF_PACKAGE_LOG_LEVEL 3
#define CONFIG_CBPRINTF_CONVERT_CHECK_PTR 1
#define CONFIG_POSIX_AEP_CHOICE_NONE 1
#define CONFIG_POSIX_C_LIB_EXT 1
#define CONFIG_POSIX_OPEN_MAX 4
#define CONFIG_POSIX_PAGE_SIZE 0x40
#define CONFIG_EVENTFD_MAX 0
#define CONFIG_MAX_PTHREAD_COUNT 0
#define CONFIG_MAX_PTHREAD_KEY_COUNT 0
#define CONFIG_MAX_TIMER_COUNT 0
#define CONFIG_MSG_COUNT_MAX 0
#define CONFIG_POSIX_LIMITS_RTSIG_MAX 0
#define CONFIG_POSIX_MAX_FDS 4
#define CONFIG_POSIX_MAX_OPEN_FILES 4
#define CONFIG_TIMER_DELAYTIMER_MAX 0
#define CONFIG_SEM_NAMELEN_MAX 0
#define CONFIG_SEM_VALUE_MAX 0
#define CONFIG_LIBGCC_RTLIB 1
#define CONFIG_RING_BUFFER 1
#define CONFIG_PRINTK 1
#define CONFIG_EARLY_CONSOLE 1
#define CONFIG_ASSERT_VERBOSE 1
#define CONFIG_LLEXT 1
#define CONFIG_LLEXT_TYPE_ELF_OBJECT 1
#define CONFIG_LLEXT_HEAP_SIZE 64
#define CONFIG_LLEXT_SHELL 1
#define CONFIG_LLEXT_SHELL_MAX_SIZE 8192
#define CONFIG_LLEXT_STORAGE_WRITABLE 1
#define CONFIG_LLEXT_EXPORT_DEVICES 1
#define CONFIG_LLEXT_LOG_LEVEL_ERR 1
#define CONFIG_LLEXT_LOG_LEVEL 1
#define CONFIG_LLEXT_EDK_NAME "llext-edk"
#define CONFIG_LOG_CORE_INIT_PRIORITY 0
#define CONFIG_LOG_MODE_IMMEDIATE 1
#define CONFIG_LOG_RUNTIME_FILTERING 1
#define CONFIG_LOG_DEFAULT_LEVEL 3
#define CONFIG_LOG_OVERRIDE_LEVEL 0
#define CONFIG_LOG_MAX_LEVEL 4
#define CONFIG_LOG_PRINTK 1
#define CONFIG_LOG_TRACE_SHORT_TIMESTAMP 1
#define CONFIG_LOG_FUNC_NAME_PREFIX_DBG 1
#define CONFIG_LOG_TAG_MAX_LEN 0
#define CONFIG_LOG_USE_VLA 1
#define CONFIG_LOG_SIMPLE_MSG_OPTIMIZE 1
#define CONFIG_LOG_ALWAYS_RUNTIME 1
#define CONFIG_LOG_OUTPUT 1
#define CONFIG_TIMER_RANDOM_INITIAL_STATE 123456789
#define CONFIG_SHELL 1
#define CONFIG_SHELL_LOG_LEVEL_DEFAULT 1
#define CONFIG_SHELL_LOG_LEVEL 3
#define CONFIG_SHELL_BACKENDS 1
#define CONFIG_SHELL_BACKEND_SERIAL 1
#define CONFIG_SHELL_BACKEND_SERIAL_INIT_PRIORITY 90
#define CONFIG_SHELL_PROMPT_UART "uart:~$ "
#define CONFIG_SHELL_BACKEND_SERIAL_INTERRUPT_DRIVEN 1
#define CONFIG_SHELL_BACKEND_SERIAL_API_INTERRUPT_DRIVEN 1
#define CONFIG_SHELL_BACKEND_SERIAL_TX_RING_BUFFER_SIZE 8
#define CONFIG_SHELL_BACKEND_SERIAL_RX_RING_BUFFER_SIZE 64
#define CONFIG_SHELL_BACKEND_SERIAL_LOG_MESSAGE_QUEUE_TIMEOUT 100
#define CONFIG_SHELL_BACKEND_SERIAL_LOG_MESSAGE_QUEUE_SIZE 512
#define CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL_DEFAULT 1
#define CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL 5
#define CONFIG_SHELL_STACK_SIZE 32768
#define CONFIG_SHELL_BACKSPACE_MODE_DELETE 1
#define CONFIG_SHELL_PROMPT_CHANGE 1
#define CONFIG_SHELL_PROMPT_BUFF_SIZE 20
#define CONFIG_SHELL_CMD_BUFF_SIZE 2048
#define CONFIG_SHELL_PRINTF_BUFF_SIZE 30
#define CONFIG_SHELL_DEFAULT_TERMINAL_WIDTH 80
#define CONFIG_SHELL_DEFAULT_TERMINAL_HEIGHT 24
#define CONFIG_SHELL_ARGC_MAX 20
#define CONFIG_SHELL_TAB 1
#define CONFIG_SHELL_TAB_AUTOCOMPLETION 1
#define CONFIG_SHELL_ASCII_FILTER 1
#define CONFIG_SHELL_WILDCARD 1
#define CONFIG_SHELL_MSG_CMD_NOT_FOUND 1
#define CONFIG_SHELL_MSG_SPECIFY_SUBCOMMAND 1
#define CONFIG_SHELL_ECHO_STATUS 1
#define CONFIG_SHELL_VT100_COMMANDS 1
#define CONFIG_SHELL_VT100_COLORS 1
#define CONFIG_SHELL_METAKEYS 1
#define CONFIG_SHELL_HELP 1
#define CONFIG_SHELL_HELP_OPT_PARSE 1
#define CONFIG_SHELL_HELP_ON_WRONG_ARGUMENT_COUNT 1
#define CONFIG_SHELL_HISTORY 1
#define CONFIG_SHELL_HISTORY_BUFFER 512
#define CONFIG_SHELL_STATS 1
#define CONFIG_SHELL_CMDS 1
#define CONFIG_SHELL_CMDS_RESIZE 1
#define CONFIG_SHELL_CMD_ROOT ""
#define CONFIG_SHELL_LOG_BACKEND 1
#define CONFIG_SHELL_LOG_FORMAT_TIMESTAMP 1
#define CONFIG_SHELL_AUTOSTART 1
#define CONFIG_SHELL_CMDS_RETURN_VALUE 1
#define CONFIG_KERNEL_SHELL 1
#define CONFIG_KERNEL_SHELL_REBOOT_DELAY 0
#define CONFIG_DEVICE_SHELL 1
#define CONFIG_DEVMEM_SHELL 1
#define CONFIG_FLASH_MAP 1
#define CONFIG_USB_DEVICE_STACK 1
#define CONFIG_USB_DEVICE_LOG_LEVEL_DEFAULT 1
#define CONFIG_USB_DEVICE_LOG_LEVEL 3
#define CONFIG_USB_DEVICE_VID 0x2341
#define CONFIG_USB_DEVICE_PID 0x0066
#define CONFIG_USB_DEVICE_MANUFACTURER "Arduino"
#define CONFIG_USB_DEVICE_PRODUCT "Arduino GIGA R1"
#define CONFIG_USB_DEVICE_SN "0123456789ABCDEF"
#define CONFIG_USB_COMPOSITE_DEVICE 1
#define CONFIG_USB_MAX_NUM_TRANSFERS 4
#define CONFIG_USB_REQUEST_BUFFER_SIZE 128
#define CONFIG_USB_MAX_ALT_SETTING 8
#define CONFIG_USB_NUMOF_EP_WRITE_RETRIES 3
#define CONFIG_USB_SELF_POWERED 1
#define CONFIG_USB_MAX_POWER 50
#define CONFIG_USB_WORKQUEUE 1
#define CONFIG_USB_WORKQUEUE_STACK_SIZE 1024
#define CONFIG_USB_WORKQUEUE_PRIORITY -1
#define CONFIG_USB_CDC_ACM 1
#define CONFIG_USB_CDC_ACM_RINGBUF_SIZE 512
#define CONFIG_CDC_ACM_INTERRUPT_EP_MPS 16
#define CONFIG_CDC_ACM_BULK_EP_MPS 64
#define CONFIG_CDC_ACM_TX_DELAY_MS 100
#define CONFIG_CDC_ACM_IAD 1
#define CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT 1
#define CONFIG_USB_CDC_ACM_LOG_LEVEL_DEFAULT 1
#define CONFIG_USB_CDC_ACM_LOG_LEVEL 3
#define CONFIG_TOOLCHAIN_ZEPHYR_0_16 1
#define CONFIG_TOOLCHAIN_ZEPHYR_SUPPORTS_THREAD_LOCAL_STORAGE 1
#define CONFIG_TOOLCHAIN_ZEPHYR_SUPPORTS_GNU_EXTENSIONS 1
#define CONFIG_LINKER_ORPHAN_SECTION_WARN 1
#define CONFIG_FLASH_LOAD_SIZE 0xc0000
#define CONFIG_ROM_END_OFFSET 0x0
#define CONFIG_LD_LINKER_SCRIPT_SUPPORTED 1
#define CONFIG_LD_LINKER_TEMPLATE 1
#define CONFIG_LINKER_SORT_BY_ALIGNMENT 1
#define CONFIG_LINKER_GENERIC_SECTIONS_PRESENT_AT_BOOT 1
#define CONFIG_LINKER_LAST_SECTION_ID 1
#define CONFIG_LINKER_LAST_SECTION_ID_PATTERN 0xE015E015
#define CONFIG_LINKER_USE_RELAX 1
#define CONFIG_LINKER_ITERABLE_SUBALIGN 4
#define CONFIG_LINKER_DEVNULL_SUPPORT 1
#define CONFIG_STD_C99 1
#define CONFIG_TOOLCHAIN_SUPPORTS_GNU_EXTENSIONS 1
#define CONFIG_SIZE_OPTIMIZATIONS 1
#define CONFIG_COMPILER_TRACK_MACRO_EXPANSION 1
#define CONFIG_COMPILER_COLOR_DIAGNOSTICS 1
#define CONFIG_FORTIFY_SOURCE_COMPILE_TIME 1
#define CONFIG_COMPILER_OPT ""
#define CONFIG_RUNTIME_ERROR_CHECKS 1
#define CONFIG_KERNEL_BIN_NAME "zephyr"
#define CONFIG_OUTPUT_STAT 1
#define CONFIG_OUTPUT_PRINT_MEMORY_USAGE 1
#define CONFIG_BUILD_OUTPUT_STRIP_PATHS 1
#define CONFIG_CHECK_INIT_PRIORITIES 1
#define CONFIG_WARN_DEPRECATED 1
#define CONFIG_ENFORCE_ZEPHYR_STDINT 1
#define CONFIG_LEGACY_GENERATED_INCLUDE_PATH 1