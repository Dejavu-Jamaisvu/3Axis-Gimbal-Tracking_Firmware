#define APPLICATION_ADDRESS 0x08008000 // Sector 2 Start Address

typedef void (*pFunction)(void);

void JumpToApplication(void) {
    uint32_t JumpAddress;
    pFunction Jump;

    // Check if there is a valid stack pointer at the application address
    if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000) {
        
        // De-initialize peripherals used by bootloader (important!)
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // Get the jump address (Reset Handler) from the vector table
        JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
        Jump = (pFunction) JumpAddress;

        // Initialize user application's Stack Pointer
        __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
        
        // Jump to the application!
        Jump();
    }
}