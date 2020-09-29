/** @file   u1wire.c
    @author M. P. Hayes
    @date   16 May 20002
    @brief  Low level routines to drive Dallas universal 1 wire bus.
    @note   This only supports a single instance of a 1 wire bus.
*/

#include <stdio.h>

#include "config.h"
#include "delay.h"
#include "u1wire.h"
#include "irq.h"
#include "pio.h"

enum
{ 
    U1WIRE_READ_ROM = 0x33, U1WIRE_SKIP_ROM = 0xcc,
    U1WIRE_MATCH_ROM = 0x55,  U1WIRE_RECALL = 0xb8
};


enum
{
    U1WIRE_DELAY_OFFSET = 6,
    U1WIRE_ADDR_BYTES = 6
};


#define U1WIRE_DEBUG 1

#ifndef U1WIRE_PIO
#error U1WIRE_PIO needs to be defined in target.h
#endif


#define U1WIRE_RELEASE() pio_config_set(U1WIRE_PIO, PIO_CONFIG_PULLUP)

#define U1WIRE_DRIVE() pio_config_set(U1WIRE_PIO, PIO_CONFIG_OUTPUT_LOW)

#define U1WIRE_TEST() pio_input_get (U1WIRE_PIO)


/** Reset one wire bus.
    @return 1 if OK, 0 if no device responding, negative if error  */
int8_t
u1wire_reset (void)
{
    /* Let bus float high by tristating driver.  Note need 4.7 kohm
       pullup resistor.  Internal pullup usually too weak.  I have
       found that without the pullup resistor we can detect a device
       but not on the first attempt.  */
    U1WIRE_RELEASE ();
    DELAY_US (5);

    if (!U1WIRE_TEST ())
    {
        /* Bus won't go high.  */
        return -U1WIRE_ERR_BUS_LOW;
    }

    irq_global_disable ();

    /* Need to drive bus low for at least 480 microseconds.  We drive
       it for 250 microseconds, check that it is low, then wait
       another 250 microseconds.  This is called the reset pulse.  */
    U1WIRE_DRIVE ();
    DELAY_US (250);

    if (U1WIRE_TEST ())
    {
        U1WIRE_RELEASE ();
        irq_global_enable ();
        /* Bus won't go low.  */
        return -U1WIRE_ERR_BUS_HIGH;
    }

    DELAY_US (250);

    /* Let bus float high by tristating driver.  */
    U1WIRE_RELEASE ();

    DELAY_US (10);
    if (!U1WIRE_TEST ())
    {
        irq_global_enable ();
        /* Bus stuck low.  */
        return -U1WIRE_ERR_BUS_STUCK; 
    }

    /* The rising edge should cause the slave to respond between
       15--60 microseconds later.  It should then drive the bus low
       for 60--240 microseconds.  */
    DELAY_US (60);

    if (U1WIRE_TEST ())
    {
        irq_global_enable ();
        /* No device responding.  */
        return 0;                      
    }

    DELAY_US (10);
    if (U1WIRE_TEST ())
    {
        irq_global_enable ();
        /* Slave does not drive bus long enough.  */
        return -U1WIRE_ERR_PRESENCE_SHORT;
    }

    DELAY_US (240);
    if (!U1WIRE_TEST ())
    {
        irq_global_enable ();
        /* Slave still driving bus longer than it should.  */
        return -U1WIRE_ERR_PRESENCE_LONG;
    }

    /* The read slot must be a minimum of 480 microseconds.  */
    DELAY_US (480 - 240);
    irq_global_enable ();
    return 1;
}


/** Write a bit on the one wire bus.
    @param value bit to write  */
void
u1wire_bit_write (uint8_t value)
{
    irq_global_disable ();

    /* 1 wire devices sample the bus somewhere between 15--60
       microseconds after the bus being pulled low.  
       I've measured a 1 bit being low for 10 microseconds
       and a 0 bit being low for 70 microseconds.  */

    U1WIRE_DRIVE ();
    DELAY_US (10 - U1WIRE_DELAY_OFFSET);

    /* When writing a 1 bit, the bus must be released within 15
       microseconds of pulling the bus low.  */
    if (value)
        U1WIRE_RELEASE ();

    /* Write slots must be a minimum of 60 microseconds in length.  */
    DELAY_US (60 - U1WIRE_DELAY_OFFSET);
    U1WIRE_RELEASE ();

    /* Need a recovery time of at least 1 microsecond between write pulses. 
       The function call/return time will exceed this.  */
    irq_global_enable ();
}


/** Write a byte on the one wire bus.
    @param value byte to write.  */
void
u1wire_byte_write (uint8_t value)
{
    int8_t i;

    /* Send LSB first.  */
    for (i = 0; i < 8; i++)
    {
        u1wire_bit_write (value & 0x01);
        value >>= 1;
    }
}


/** Read a bit from the one wire bus.
    @return bit read  */
uint8_t
u1wire_bit_read (void)
{
    uint8_t value;

    irq_global_disable ();

    /* Generate a read pulse; need to drive the bus for at least 1
       microsecond.  */
    U1WIRE_DRIVE ();
    DELAY_US (2 - 1);
    U1WIRE_RELEASE ();

    DELAY_US (12 - U1WIRE_DELAY_OFFSET);
    /* The data must be read within 15 microseconds from the start of
       the slot.  */
    value = U1WIRE_TEST ();

    /* All read slots must be a minimum of 60 microseconds duration with a 
       recovery time of at least 1 microsecond between read pulses.  */
    DELAY_US (60);

    irq_global_enable ();
    return value;
}


/** Read a byte from the one wire bus.
    @return byte read  */
uint8_t
u1wire_byte_read (void)
{
    int8_t i;
    uint8_t value = 0;

    /* Read LSB first.  */
    for (i = 0; i < 8; i++)
    {
        value >>= 1;
        if (u1wire_bit_read ())
            value |= BIT (7);
    }
    return value;
}


/** Determine the ROM code for a single device on a one wire bus.
    @param dev pointer to one wire device structure 
    @return 1 if OK, 0 if no device responding, negative if error  */
static int8_t
u1wire_rom_code_read (u1wire_t dev)
{
    int8_t ret;

    if ((ret = u1wire_reset ()) != 1)
        return ret;

    u1wire_byte_write (U1WIRE_READ_ROM);

    /* Read family code, serial number, and CRC.  */
    u1wire_read (&dev->rom_code, sizeof (dev->rom_code));

    /* Probably should check CRC.  */

    /* Dummy function command (recall).  */
    u1wire_byte_write (U1WIRE_RECALL);
    return 1;
}


/** Send command to selected device on one wire bus.
    @param dev pointer to one wire device structure
    @param command command to send
    @return 1 if OK, 0 if no device responding, negative if error  */
int8_t
u1wire_command (u1wire_t dev, uint8_t command)
{
    int8_t ret;

    if ((ret = u1wire_reset ()) != 1)
        return ret;

    u1wire_byte_write (U1WIRE_MATCH_ROM);
    u1wire_write (&dev->rom_code, sizeof (dev->rom_code));

    u1wire_byte_write (command);

    return 1;
}


/** Broadcast command to all devices on one wire bus.
    @param command command to send
    @return 1 if OK, 0 if no device responding, negative if error  */
int8_t
u1wire_broadcast (uint8_t command)
{
    int8_t ret;

    if ((ret = u1wire_reset ()) != 1)
        return ret;

    /* Broadcast to all devices.  */
    u1wire_byte_write (U1WIRE_SKIP_ROM);

    u1wire_byte_write (command);

    return 1;
}


/** Read number of bytes of data from one wire bus.
    @param data pointer to buffer to write to
    @param size number of bytes to write
    @return number of bytes read, 0 if no device responding, negative if error  */
int8_t
u1wire_read (void *data, uint8_t size)
{
    uint8_t i;
    uint8_t *buffer = data;

    for (i = 0; i < size; i++)
        *buffer++ = u1wire_byte_read ();

    return i;
}


/** Write number of bytes of data from one wire bus.
    @param data pointer to buffer to write to
    @param size number of bytes to write
    @return number of bytes written, 0 if no device responding, negative if error  */
int8_t
u1wire_write (void *data, uint8_t size)
{
    uint8_t i;
    uint8_t *buffer = data;

    for (i = 0; i < size; i++)
        u1wire_byte_write (*buffer++);

    return i;
}


/** Determine if one wire bus operation has completed.
    @return 1 if one wire bus operation has completed otherwise 0  */
bool
u1wire_ready_p (void)
{
    return u1wire_bit_read ();
}


/** Initialise one wire bus and discover if there is a single device on the bus.
    @param device pointer to one wire device structure
    @return 1 if OK, 0 if no device responding, negative if error  */
int8_t
u1wire_init (u1wire_obj_t *device)
{
    U1WIRE_RELEASE ();

    return u1wire_rom_code_read (device);
}
