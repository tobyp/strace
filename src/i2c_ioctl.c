/*
 * Copyright (c) 2021 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "xlat/i2c_funcs.h"

static int
print_i2c_funcs(struct tcb *const tcp, const kernel_ulong_t arg)
{
   kernel_ulong_t funcs;

   if (entering(tcp))
       return 0;

   tprint_arg_next();
   if (umove_or_printaddr(tcp, arg, &funcs))
       return RVAL_IOCTL_DECODED;

   printflags(i2c_funcs, funcs, NULL);
   return RVAL_IOCTL_DECODED;
}

#include "xlat/i2c_msg_flags.h"

static void
print_i2c_msg(struct tcb *const tcp, const struct i2c_msg *const sd)
{
   tprint_struct_begin();
   PRINT_FIELD_X(*sd, addr);
   tprint_struct_next();
   PRINT_FIELD_FLAGS(*sd, flags, i2c_msg_flags, "I2C_M_???");
   tprint_struct_next();
   PRINT_FIELD_U(*sd, len);
   tprint_struct_next();
   tprints_field_name("buf");

   __u8 * buf = xmalloc(sd->len);
   if (!umoven_or_printaddr(tcp, (kernel_ulong_t)sd->buf, sd->len, buf)) {
       print_quoted_string((const char *)buf, sd->len,
                   QUOTE_FORCE_HEX);
   }
   free(buf);
   tprint_struct_end();
}

static int
print_i2c_rdwr(struct tcb *const tcp, const kernel_ulong_t arg)
{
   struct i2c_rdwr_ioctl_data rwd;
   struct i2c_msg msg;

   if (entering(tcp))
       tprint_arg_next();
   else if (syserror(tcp))
       return RVAL_IOCTL_DECODED;
   else
       tprint_value_changed();

   if (umove_or_printaddr(tcp, arg, &rwd))
       return RVAL_IOCTL_DECODED;

   if (entering(tcp)) {
       tprint_struct_begin();

       PRINT_FIELD_D(rwd, nmsgs);
       tprint_struct_next();

       tprint_array_begin();
       for (unsigned long i=0; i<rwd.nmsgs; ++i) {
           if (i != 0) tprint_array_next();
           if (!umove_or_printaddr(tcp,
                       (kernel_ulong_t)(rwd.msgs + i),
                       &msg)) {
               print_i2c_msg(tcp, &msg);
           }
       }
       tprint_array_end();

       return 0;
   }

   /* exiting */
   tprint_array_begin();
   for (unsigned long i=0; i<rwd.nmsgs; ++i) {
       if (i != 0) tprint_array_next();
       if (!umove_or_printaddr(tcp, (kernel_ulong_t)(rwd.msgs + i),
                   &msg)) {
           print_i2c_msg(tcp, &msg);
       }
   }
   tprint_array_end();
   tprint_struct_end();

   return RVAL_IOCTL_DECODED;
}

#include "xlat/i2c_smbus_read_write.h"
#include "xlat/i2c_smbus_size.h"

#include <stdio.h>

/* For QUICK actions and single-byte writes, the data field is ignored. */
static int
i2c_smbus_data_matters(const struct i2c_smbus_ioctl_data *const sd)
{
   return !(sd->size == I2C_SMBUS_QUICK ||
           ((sd->size == I2C_SMBUS_BYTE) &&
            (sd->read_write == I2C_SMBUS_WRITE)));
}

/* The kernel only changes data on procedure calls or reads. */
static int
i2c_smbus_data_mutation(const struct i2c_smbus_ioctl_data *const sd)
{
   return sd->size == I2C_SMBUS_PROC_CALL ||
          sd->size == I2C_SMBUS_BLOCK_PROC_CALL ||
          sd->read_write == I2C_SMBUS_READ;
}

static int
print_i2c_smbus(struct tcb *const tcp, const kernel_ulong_t arg)
{
   struct i2c_smbus_ioctl_data sd;
   union i2c_smbus_data d;

   if (entering(tcp))
       tprint_arg_next();
   else if (syserror(tcp))
       return RVAL_IOCTL_DECODED;

   if (umove_or_printaddr(tcp, arg, &sd))
       return RVAL_IOCTL_DECODED;

   if (entering(tcp)) {
       tprint_struct_begin();

       PRINT_FIELD_XVAL(sd, read_write, i2c_smbus_read_write,
                "I2C_SMBUS_???");
       tprint_struct_next();

       PRINT_FIELD_X(sd, command);
       tprint_struct_next();

       PRINT_FIELD_XVAL(sd, size, i2c_smbus_size, "I2C_SMBUS_???");
       tprint_struct_next();

       if (!i2c_smbus_data_matters(&sd)) {
           /* in this case data is not used at all, only command */
           tprints_field_name("data");
           tprint_more_data_follows();
           tprint_struct_end();  // struct i2c_smbus_ioctl_data
           return RVAL_IOCTL_DECODED;
       }

       tprints_field_name("data");
       if (umove_or_printaddr(tcp, (kernel_ulong_t)sd.data, &d))
           return RVAL_IOCTL_DECODED;
       tprint_struct_begin();
       if (sd.size == I2C_SMBUS_BYTE_DATA ||
           sd.size == I2C_SMBUS_BYTE)
           PRINT_FIELD_X(d, byte);
       else if (sd.size == I2C_SMBUS_WORD_DATA ||
            sd.size == I2C_SMBUS_PROC_CALL)
           PRINT_FIELD_X(d, word);
       else
           PRINT_FIELD_HEX_ARRAY(d, block);
       tprint_struct_end();

       return 0;
   }

   /* exiting */
   if (i2c_smbus_data_mutation(&sd)) {
       tprint_value_changed();
       if (!umove_or_printaddr(tcp, (kernel_ulong_t)sd.data, &d)) {
           tprint_struct_begin();
           if (sd.size == I2C_SMBUS_BYTE_DATA ||
               sd.size == I2C_SMBUS_BYTE)
               PRINT_FIELD_X(d, byte);
           else if (sd.size == I2C_SMBUS_WORD_DATA ||
                sd.size == I2C_SMBUS_PROC_CALL)
               PRINT_FIELD_X(d, word);
           else
               PRINT_FIELD_HEX_ARRAY(d, block);
           tprint_struct_end();
       }
   }

   tprint_struct_end();  // struct i2c_smbus_ioctl_data

   return RVAL_IOCTL_DECODED;
}

int
i2c_ioctl(struct tcb *const tcp, const unsigned int code,
     const kernel_ulong_t arg)
{
   switch (code) {
   /* numeric arguments; I2C_TIMEOUT is in units of 10ms */
   case I2C_RETRIES:
   case I2C_TIMEOUT:
       tprint_arg_next();
       PRINT_VAL_D((kernel_long_t)arg);
       return RVAL_IOCTL_DECODED;

   /* booleans (==0 as false, !=0 as true) */
   case I2C_PEC:
   case I2C_TENBIT:
       tprint_arg_next();
       PRINT_VAL_D((kernel_long_t)arg);
       return RVAL_IOCTL_DECODED;

   /* I2C Addresses */
   case I2C_SLAVE:
   case I2C_SLAVE_FORCE:
       tprint_arg_next();
       PRINT_VAL_X((kernel_long_t)arg);
       return RVAL_IOCTL_DECODED;

   /* Flags */
   case I2C_FUNCS:
       return print_i2c_funcs(tcp, arg);

   /* Structures */
   case I2C_RDWR:
       return print_i2c_rdwr(tcp, arg);
   case I2C_SMBUS:
       return print_i2c_smbus(tcp, arg);
   }
   return RVAL_DECODED;
}
