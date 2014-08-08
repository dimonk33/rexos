/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2014, Alexey Kramarenko
    All rights reserved.
*/

#ifndef DBG_H
#define DBG_H

/** \addtogroup debug debug routines
    \{
    debug routines
 */

/*
        dbg.h: debug-specific
  */


#define MAGIC_TIMER                                    0xbecafcf5
#define MAGIC_PROCESS                                  0x7de32076
#define MAGIC_MUTEX                                    0xd0cc6e26
#define MAGIC_EVENT                                    0x57e198c7
#define MAGIC_SEM                                      0xabfd92d9
#define MAGIC_STREAM                                   0xf4eb741c
#define MAGIC_STREAM_HANDLE                            0x250b73c2
#define MAGIC_BLOCK                                    0x890f6c75

#define MAGIC_UNINITIALIZED                            0xcdcdcdcd
#define MAGIC_UNINITIALIZED_BYTE                       0xcd

#if !defined(LDS) && !defined(__ASSEMBLER__)

#include "kernel_config.h"
#include "../userspace/lib/stdlib.h"
#include "../userspace/cc_macro.h"

#if (KERNEL_ASSERTIONS)


/**
    \brief halts system macro
    \details only works, if \ref KERNEL_INFO is set
    \retval no return
*/
#define HALT()                                           {for (;;) {}}

/**
    \brief debug assertion
    \details only works, if \ref KERNEL_INFO is set.

    prints over debug console file name and line, caused assertion
    \param cond: assertion made if \b cond is \b false
    \retval no return if not \b cond, else none
*/
#define ASSERT(cond)                                    if (!(cond))    {printk("ASSERT at %s, line %d\n\r", __FILE__, __LINE__);    HALT();}

#else

#define HALT()
#define ASSERT(cond)

#endif

#if (KERNEL_MARKS)

/**
    \brief check, if object mark is right (object is valid)
    \details only works, if \ref KERNEL_INFO and \ref KERNEL_MARKS are set.
    \param obj: object to check
    \param magic_value: value to set. check \ref magic.h for details
    \param name: object text to display in case of wrong magic
    \retval no return if wrong magic, else none
*/
#if (KERNEL_ASSERTIONS)
#define CHECK_MAGIC(obj, magic_value)    if ((obj)->magic != (magic_value)) {printk("INVALID MAGIC at %s, line %d\n\r", __FILE__, __LINE__);    HALT();}
#define CHECK_MAGIC_RET(obj, magic_value, ret)    if ((obj)->magic != (magic_value)) {printk("INVALID MAGIC at %s, line %d\n\r", __FILE__, __LINE__);    HALT();}
#else
#define CHECK_MAGIC(obj, magic_value)    if ((obj)->magic != (magic_value)) {kprocess_error_current(ERROR_INVALID_MAGIC); return;}
#define CHECK_MAGIC_RET(obj, magic_value, ret)    if ((obj)->magic != (magic_value)) {kprocess_error_current(ERROR_INVALID_MAGIC); return ret;}
#endif
/**
    \brief apply object magic on object creation
    \details only works, if \ref KERNEL_INFO and \ref KERNEL_MARKS are set.
    \param obj: object to check
    \param magic_value: value to set. check \ref magic.h for details
    \retval none
*/
#define DO_MAGIC(obj, magic_value)                    (obj)->magic = (magic_value)
/**
    \brief this macro must be put in object structure
*/

#define CLEAR_MAGIC(obj)                              (obj)->magic = 0
/**
    \brief this macro must be put in object structure
*/
#define MAGIC                                         unsigned int magic

#else
#define CHECK_MAGIC(obj, magic_vale)
#define CHECK_MAGIC_RET(obj, magic_vale)
#define DO_MAGIC(obj, magic_value)
#define CLEAR_MAGIC(obj)
#define MAGIC
#endif //KERNEL_MARKS

#if (KERNEL_HANDLE_CHECKING)
#if (KERNEL_ASSERTIONS)
#define CHECK_HANDLE(handle, size)     if (((unsigned int)(handle) < (SRAM_BASE) + (KERNEL_GLOBAL_SIZE)) \
                                       || ((unsigned int)(handle) + (size) >= (SRAM_BASE) + (KERNEL_SIZE))) \
                                          {printk("INVALID HANDLE at %s, line %d\n\r", __FILE__, __LINE__);    HALT();}
#define CHECK_HANDLE_RET(handle, size, ret)     if (((unsigned int)(handle) < (SRAM_BASE) + (KERNEL_GLOBAL_SIZE)) \
                                       || ((unsigned int)(handle) + (size) >= (SRAM_BASE) + (KERNEL_SIZE))) \
                                          {printk("INVALID HANDLE at %s, line %d\n\r", __FILE__, __LINE__);    HALT();}
#else
#define CHECK_HANDLE(handle, size)     if (((unsigned int)(handle) < (SRAM_BASE) + (KERNEL_GLOBAL_SIZE)) \
                                       || ((unsigned int)(handle) + (size) >= (SRAM_BASE) + (KERNEL_SIZE))) \
                                          {kprocess_error_current(ERROR_INVALID_MAGIC); return;}
#define CHECK_HANDLE_RET(handle, size, ret)     if (((unsigned int)(handle) < (SRAM_BASE) + (KERNEL_GLOBAL_SIZE)) \
                                       || ((unsigned int)(handle) + (size) >= (SRAM_BASE) + (KERNEL_SIZE))) \
                                          {kprocess_error_current(ERROR_INVALID_MAGIC); return ret;}
#endif
#else
#define CHECK_HANDLE(handle, size)
#define CHECK_HANDLE_RET(handle, size, ret)
#endif //KERNEL_HANDLE_CHECKING

#if (KERNEL_ADDRESS_CHECKING)
#if (KERNEL_ASSERTIONS)
#define CHECK_ADDRESS(process, address, sz)     if ((process != 0 && __KERNEL->context < 0) && \
                                                    (((unsigned int)(address) < (unsigned int)(((PROCESS*)(process))->heap)) \
                                                    || ((unsigned int)(address) + (sz) >= (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size))) \
                                                     {printk("INVALID ADDRESS at %s, line %d, process: %s\n\r", __FILE__, __LINE__, PROCESS_NAME(((PROCESS*)(process))->heap));    HALT();}
#define CHECK_ADDRESS_RET(process, address, sz, ret)     if ((process != 0 && __KERNEL->context < 0) && \
                                                    (((unsigned int)(address) < (unsigned int)(((PROCESS*)(process))->heap)) \
                                                    || ((unsigned int)(address) + (sz) >= (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size))) \
                                                     {printk("INVALID ADDRESS at %s, line %d, process: %s\n\r", __FILE__, __LINE__, PROCESS_NAME(((PROCESS*)(process))->heap));    HALT();}
#define CHECK_ADDRESS_FLASH(process, address, sz)     if ((process != 0 && __KERNEL->context < 0) && \
                                                        !((((unsigned int)(address) >= (unsigned int)(((PROCESS*)(process))->heap)) \
                                                        && ((unsigned int)(address) + (sz) < (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size)) || \
                                                        (((unsigned int)(address) >= FLASH_BASE) \
                                                        && ((unsigned int)(address) + (sz) < FLASH_BASE + FLASH_SIZE)))) \
                                                            {printk("INVALID ADDRESS at %s, line %d, process: %s\n\r", __FILE__, __LINE__, PROCESS_NAME(((PROCESS*)(process))->heap));    HALT();}
#define CHECK_ADDRESS_FLASH_RET(process, address, sz, ret)     if ((process != 0 && __KERNEL->context < 0) && \
                                                        !((((unsigned int)(address) >= (unsigned int)(((PROCESS*)(process))->heap)) \
                                                        && ((unsigned int)(address) + (sz) < (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size)) || \
                                                        (((unsigned int)(address) >= FLASH_BASE) \
                                                        && ((unsigned int)(address) + (sz) < FLASH_BASE + FLASH_SIZE)))) \
                                                            {printk("INVALID ADDRESS at %s, line %d, process: %s\n\r", __FILE__, __LINE__, PROCESS_NAME(((PROCESS*)(process))->heap));    HALT();}
#else
#define CHECK_ADDRESS(process, address, sz)     if ((process != 0 && __KERNEL->context < 0) && \
                                                   (((unsigned int)(address) < (unsigned int)(((PROCESS*)(process))->heap)) \
                                                   || ((unsigned int)(address) + (sz) >= (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size))) \
                                                      {kprocess_error_current(ERROR_ACCESS_DENIED); return;}
#define CHECK_ADDRESS_RET(process, address, sz, ret)     if ((process != 0 && __KERNEL->context < 0) && \
                                                   (((unsigned int)(address) < (unsigned int)(((PROCESS*)(process))->heap)) \
                                                   || ((unsigned int)(address) + (sz) >= (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size))) \
                                                      {kprocess_error_current(ERROR_ACCESS_DENIED); return ret;}
#define CHECK_ADDRESS_FLASH(process, address, sz)     if ((process != 0 && __KERNEL->context < 0) && \
                                                        !((((unsigned int)(address) >= (unsigned int)(((PROCESS*)(process))->heap)) \
                                                        && ((unsigned int)(address) + (sz) < (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size)) || \
                                                        (((unsigned int)(address) >= FLASH_BASE) \
                                                        && ((unsigned int)(address) + (sz) < FLASH_BASE + FLASH_SIZE)))) \
                                                            {kprocess_error_current(ERROR_ACCESS_DENIED); return;}
#define CHECK_ADDRESS_FLASH_RET(process, address, ret)     if ((process != 0 && __KERNEL->context < 0) && \
                                                        !((((unsigned int)(address) >= (unsigned int)(((PROCESS*)(process))->heap)) \
                                                        && ((unsigned int)(address) + (sz) < (unsigned int)(((PROCESS*)(process))->heap) + ((PROCESS*)(process))->size)) || \
                                                        (((unsigned int)(address) >= FLASH_BASE) \
                                                        && ((unsigned int)(address) + (sz) < FLASH_BASE + FLASH_SIZE)))) \
                                                            {kprocess_error_current(ERROR_ACCESS_DENIED); return ret;}
#endif //KERNEL_ASSERTIONS
#else
#define CHECK_ADDRESS(process, address, size)
#define CHECK_ADDRESS_RET(process, address, size, ret)
#define CHECK_ADDRESS_FLASH(process, address, size)
#define CHECK_ADDRESS_FLASH_RET(process, address, size, ret)
#endif //KERNEL_ADDRESS_CHECKING

/**
    \brief format string, using kernel stdio as handler
    \param fmt: format (see global description)
    \param ...: list of arguments
    \retval none
*/
void printk(const char *const fmt, ...);

/**
    \brief print minidump
    \param addr: starting address
    \param size: size of dump
    \retval none
*/
void dump(unsigned int addr, unsigned int size);


#endif // !defined(LDS) && !defined(__ASSEMBLER__)

#endif // DBG_H
