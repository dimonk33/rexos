/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2014, Alexey Kramarenko
    All rights reserved.
*/

#include "lpc_timer.h"
#include "lpc_core_private.h"
#include "lpc_power.h"
#include "lpc_config.h"
#include "../../../userspace/timer.h"
#include "../../../userspace/irq.h"
#if (SYS_INFO)
#include "../../../userspace/lib/stdio.h"
#endif

#define TC16PC                                      15
#define S1_US                                       1000000

static const uint8_t __TIMER_POWER_PINS[] =         {SYSCON_SYSAHBCLKCTRL_CT16B0_POS, SYSCON_SYSAHBCLKCTRL_CT16B1_POS,
                                                     SYSCON_SYSAHBCLKCTRL_CT32B0_POS, SYSCON_SYSAHBCLKCTRL_CT32B1_POS};
static const uint8_t __TIMER_VECTORS[] =            {16, 17, 18, 19};

typedef LPC_CTxxBx_Type* LPC_CTxxBx_Type_P;

static const LPC_CTxxBx_Type_P __TIMER_REGS[] =     {LPC_CT16B0, LPC_CT16B1, LPC_CT32B0, LPC_CT32B1};

void lpc_timer_isr(int vector, void* param)
{
    if ((__TIMER_REGS[SECOND_TIMER]->IR & (1 << HPET_CHANNEL)) && (__TIMER_REGS[SECOND_TIMER]->MCR & (1 << (HPET_CHANNEL * 3))))
    {
        __TIMER_REGS[SECOND_TIMER]->IR = 1 << HPET_CHANNEL;
        timer_hpet_timeout();
    }
    if (__TIMER_REGS[SECOND_TIMER]->IR & CT_IR_MR0INT)
    {
        __TIMER_REGS[SECOND_TIMER]->IR = CT_IR_MR0INT;
        timer_second_pulse();
    }
}

void lpc_timer_open(CORE* core, TIMER timer, unsigned int flags)
{
    if (timer >= TIMER_MAX)
    {
        error(ERROR_INVALID_PARAMS);
        return;
    }
    //enable clock
    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << __TIMER_POWER_PINS[timer];

    if (flags & TIMER_FLAG_ENABLE_IRQ)
    {
        //clear pending interrupts
        NVIC_ClearPendingIRQ(__TIMER_VECTORS[timer]);
        if (timer & 1)
            __TIMER_REGS[timer]->IR = CT_IR_MR0INT | CT_IR_MR1INT | CT_IR_MR2INT | CT_IR_MR3INT | CT_IR_CR0INT | CT1_IR_CR1INT;
        else
            __TIMER_REGS[timer]->IR = CT_IR_MR0INT | CT_IR_MR1INT | CT_IR_MR2INT | CT_IR_MR3INT | CT_IR_CR0INT | CT0_IR_CR1INT;
        //enable interrupts
        NVIC_EnableIRQ(__TIMER_VECTORS[timer]);
        NVIC_SetPriority(__TIMER_VECTORS[timer], (flags & TIMER_FLAG_PRIORITY_MASK) >> TIMER_FLAG_PRIORITY_POS);
    }
}

void lpc_timer_close(CORE* core, TIMER timer)
{
    if (timer >= TIMER_MAX)
    {
        error(ERROR_INVALID_PARAMS);
        return;
    }
    //disable interrupts
    NVIC_DisableIRQ(__TIMER_VECTORS[timer]);
    //disable clock
    LPC_SYSCON->SYSAHBCLKCTRL &= ~(1 << __TIMER_POWER_PINS[timer]);
}


void lpc_timer_start(CORE* core, TIMER timer, unsigned int mode, unsigned int value)
{
    if (timer >= TIMER_MAX)
    {
        error(ERROR_NOT_SUPPORTED);
        return;
    }
    int channel = (mode & TIMER_CHANNEL_MASK) >> TIMER_CHANNEL_POS;

    if (channel)
    {
        //value must be less, than channel 0 count
        unsigned int match = __TIMER_REGS[timer]->TC + (value > __TIMER_REGS[timer]->MR[0] ? __TIMER_REGS[timer]->MR[0] : value);
        if (match > __TIMER_REGS[timer]->MR[0])
            match -= (__TIMER_REGS[timer]->MR[0] + 1);
         __TIMER_REGS[timer]->MR[channel] = match;
        //enable interrupt on match channel
        __TIMER_REGS[timer]->MCR |= 1 << (channel * 3);
    }
    else
    {
        unsigned int clock = lpc_power_get_system_clock_inside(core);
        __TIMER_REGS[timer]->TC = __TIMER_REGS[timer]->PC = 0;
        //for 32 bit timers routine is different and much more easy
        if (timer >= TC32B0)
        {
            switch(mode & TIMER_MODE_MASK)
            {
            case TIMER_MODE_US:
                //set PC to 1 us
                __TIMER_REGS[timer]->PR = clock / 1000000 - 1;
                __TIMER_REGS[timer]->MR[0] = value - 1;
                break;
            case TIMER_MODE_HZ:
                //set PC to 1 us
                __TIMER_REGS[timer]->PR = clock / 1000000 - 1;
                __TIMER_REGS[timer]->MR[0] = 1000000 / value - 1;
                break;
            default:
                //set PC to 1 CLK
                __TIMER_REGS[timer]->PR = 0;
                __TIMER_REGS[timer]->MR[0] = value - 1;
                break;
            }
        }
        else
        {
            //convert value to clock units
            unsigned int cu = 0;
            switch(mode & TIMER_MODE_MASK)
            {
            case TIMER_MODE_US:
                cu = clock / 1000000 * value;
                break;
            case TIMER_MODE_HZ:
                cu = clock / value;
                break;
            default:
                cu = value;
                break;
            }
            unsigned int psc = cu / 0x10000;
            if (psc % 0x10000 >= 0x8000)
                psc++;
            __TIMER_REGS[timer]->PR = psc - 1;
            __TIMER_REGS[timer]->MR[0] = (cu / psc) - 1;
        }
        //enable interrupt on match channel
        if (mode & TIMER_MODE_ONE_PULSE)
            __TIMER_REGS[timer]->MCR |= CT_MCR_MR0I | CT_MCR_MR0S;
        else
            __TIMER_REGS[timer]->MCR |= CT_MCR_MR0I | CT_MCR_MR0R;
        //start counter
        __TIMER_REGS[timer]->TCR |= CT_TCR_CEN;
    }
}

void lpc_timer_stop(CORE* core, TIMER timer, unsigned int mode)
{
    if (timer >= TIMER_MAX)
    {
        error(ERROR_NOT_SUPPORTED);
        return;
    }
    int channel = (mode & TIMER_CHANNEL_MASK) >> TIMER_CHANNEL_POS;

    //disable match interrupt
    __TIMER_REGS[timer]->MCR &= ~(7 << (channel * 3));
    //disable timer on channel 0 disable
    if (channel == 0)
        __TIMER_REGS[timer]->TCR &= ~CT_TCR_CEN;
    //clear pending match interrupt
    __TIMER_REGS[timer]->IR = 1 << channel;
}

void hpet_start(unsigned int value, void* param)
{
    CORE* core = (CORE*)param;
    core->timer.hpet_start = __TIMER_REGS[SECOND_TIMER]->TC;
    //don't need to start in free-run mode, second pulse will go faster anyway
    if (value < S1_US)
       lpc_timer_start(core, SECOND_TIMER, TIMER_MODE_US | HPET_CHANNEL, SECOND_TIMER >= TC32B0 ? value : value / TC16PC);
}

void hpet_stop(void* param)
{
    CORE* core = (CORE*)param;
    lpc_timer_stop(core, SECOND_TIMER, HPET_CHANNEL);
}

unsigned int hpet_elapsed(void* param)
{
    CORE* core = (CORE*)param;
    unsigned int tc = __TIMER_REGS[SECOND_TIMER]->TC;
    unsigned int value = core->timer.hpet_start < tc ? tc - core->timer.hpet_start : __TIMER_REGS[SECOND_TIMER]->MR[0] + 1 - core->timer.hpet_start + tc;
    return SECOND_TIMER >= TC32B0 ? value : value / TC16PC;
}

void lpc_timer_init(CORE *core)
{
    core->timer.hpet_start = 0;

    //setup second tick timer
    irq_register(__TIMER_VECTORS[SECOND_TIMER], lpc_timer_isr, (void*)core);
    lpc_timer_open(core, SECOND_TIMER, TIMER_FLAG_ENABLE_IRQ | (13 << TIMER_FLAG_PRIORITY_POS));
    lpc_timer_start(core, SECOND_TIMER, TIMER_MODE_US | TIMER_CHANNEL0, S1_US);

    CB_SVC_TIMER cb_svc_timer;
    cb_svc_timer.start = hpet_start;
    cb_svc_timer.stop = hpet_stop;
    cb_svc_timer.elapsed = hpet_elapsed;
    timer_setup(&cb_svc_timer, core);
}


#if (SYS_INFO)
void lpc_timer_info()
{
    printd("System timer: TC%dB%d\n\r", SECOND_TIMER >= TC32B0 ? 32 : 16, SECOND_TIMER & 1);
}
#endif

bool lpc_timer_request(CORE* core, IPC* ipc)
{
    bool need_post = false;
    switch (ipc->cmd)
    {
#if (SYS_INFO)
    case IPC_GET_INFO:
        lpc_timer_info();
        need_post = true;
        break;
#endif
    case IPC_OPEN:
        lpc_timer_open(core, (TIMER)HAL_ITEM(ipc->param1), ipc->param2);
        need_post = true;
        break;
    case IPC_CLOSE:
        lpc_timer_close(core, (TIMER)HAL_ITEM(ipc->param1));
        need_post = true;
        break;
    case LPC_TIMER_START:
        lpc_timer_start(core, (TIMER)HAL_ITEM(ipc->param1), ipc->param1, ipc->param3);
        need_post = true;
        break;
    case LPC_TIMER_STOP:
        lpc_timer_stop(core, (TIMER)HAL_ITEM(ipc->param1), ipc->param2);
        need_post = true;
        break;
    default:
        ipc_set_error(ipc, ERROR_NOT_SUPPORTED);
        need_post = true;
        break;
    }
    return need_post;
}
