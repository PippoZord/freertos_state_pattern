add_executable(${NAME}
        main.c
        context.c
        state.c
        runtime.c
        agent.c
        peripheral.c
        blinkled.c
        output.c
        input.c
        pwm.c
        togglebutton.c
        fadeled.c
        uart.c
        thyonei.c
        )

# Header search path for our own .h files (project_root/include)
target_include_directories(${NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    )

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(${NAME}
    pico_stdlib
    hardware_pwm # for pwm.c (hardware/pwm.h)
    hardware_uart # for pwm.c (hardware/pwm.h)
	FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
    freertos_config
    )

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})


# enable usb output, disable uart output
pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 1)
target_compile_definitions(${NAME} PRIVATE
    PICO_DEFAULT_UART_RX_PIN=17
    PICO_DEFAULT_UART_TX_PIN=16
    # stdio_init_all() aspetta la connessione USB CDC (invece di scrivere nel
    # vuoto e perdere i primi byte se il terminale non e' ancora collegato).
    # -1 = attesa indefinita.
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=-1
)
