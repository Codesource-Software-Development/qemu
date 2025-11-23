/*
 * tpm_tis_spi.c - QEMU's TPM TIS SPI Device
 *
 * Copyright (c) 2025 Codesource Software Development
 *
 * Authors:
 *   Lambert Duijst <oss@codesourcedev.nl>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/ssi/ssi.h"
#include "hw/sysbus.h"
#include "hw/acpi/tpm.h"
#include "migration/vmstate.h"
#include "tpm_prop.h"
#include "qemu/log.h"
#include "trace.h"
#include "tpm_tis.h"


/* TPM TIS SPI registers, based on drivers/char/tpm/tpm_tis_core.h */
#define	TPM_ACCESS			(0x0000)
#define	TPM_INT_ENABLE		(0x0008)
#define	TPM_INT_VECTOR		(0x000C)
#define	TPM_INT_STATUS		(0x0010)
#define	TPM_INTF_CAPS		(0x0014)
#define	TPM_STS		(0x0018)
#define	TPM_STS3			(0x001b)
#define	TPM_DATA_FIFO		(0x0024)

#define	TPM_DID_VID(l)			(0x0F00 | ((l) << 12))
#define	TPM_RID(l)			(0x0F04 | ((l) << 12))

// Notes from studying tpm_tis_i2c.c
// FIFO Reads/writes (so to TPM_DATA_FIFO) , immediately done, no caching
// When being read through tpm_tis_read_data, mask return value with & 0xff, LSB is the value
// To send back

/*
 *
 * This tpm_tis_spi driver is eventually supposed to work using
 * the following qemu command line.
 *
 *
 *
 * -chardev socket,id=chrtpm,path=/tmp/mytpm/sock \
 * -tpmdev emulator,id=tpm0,chardev=chrtpm \
 * -device tpm-tis-spi,spi-bus=spi1,cs=0,tpmdev=tpm0
 *
 */

#define TYPE_TPM_TIS_SPI "tpm-tis-spi"

#define TPM_TIS_SPI_IS_VALID_LOCTY(x)   TPM_TIS_IS_VALID_LOCTY(x)

typedef struct TPMStateSPI {
    /*< private >*/
    SSIPeripheral parent_obj;
    TPMState state;
    BusState qbus;

    int in_header;
    int header_cnt;
    int is_read;

    //uint8_t transfer_bytes_left;
    uint8_t transfer_size;
    uint8_t write_len;
    //uint8_t dummy_write_count;

    uint64_t addr;

    uint32_t tpm_transfer_value; // Used for both reads and writes

} TPMStateSPI;

OBJECT_DECLARE_SIMPLE_TYPE(TPMStateSPI, TPM_TIS_SPI)

static int tpm_tis_spi_pre_save(void *opaque)
{
    TPMStateSPI *spist = opaque;

    return tpm_tis_pre_save(&spist->state);
}

static int tpm_tis_spi_post_load(void *opaque, int version_id)
{
    TPMStateSPI *spist = opaque;
    (void) spist;
    // @TODO : Implement according to SPI
    //if (spist->offset >= 1) {
    //    tpm_tis_spi_to_tis_reg(spist, spist->data[0]);
    //}

    return 0;
}

// @TODO : Implement according to SPI
static const VMStateDescription vmstate_tpm_tis_spi = {
    .name = "tpm-tis-spi",
    .version_id = 0,
    .pre_save  = tpm_tis_spi_pre_save,
    .post_load  = tpm_tis_spi_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_BUFFER(state.buffer, TPMStateSPI),
        VMSTATE_UINT16(state.rw_offset, TPMStateSPI),
        VMSTATE_UINT8(state.active_locty, TPMStateSPI),
        VMSTATE_UINT8(state.aborting_locty, TPMStateSPI),
        VMSTATE_UINT8(state.next_locty, TPMStateSPI),

        VMSTATE_STRUCT_ARRAY(state.loc, TPMStateSPI, TPM_TIS_NUM_LOCALITIES, 0,
                             vmstate_locty, TPMLocality),

        /* spi specifics */
        // @TODO : Implement according to SPI
        //VMSTATE_UINT8(offset, TPMStateSPI),
        //VMSTATE_UINT8(operation, TPMStateSPI),
        //VMSTATE_BUFFER(data, TPMStateSPI),
        //VMSTATE_UINT8(loc_sel, TPMStateSPI),
        //VMSTATE_UINT8(csum_enable, TPMStateSPI),

        VMSTATE_END_OF_LIST()
    }
};

static const Property tpm_tis_spi_properties[] = {
    DEFINE_PROP_TPMBE("tpmdev", TPMStateSPI, state.be_driver),
};

static void tpm_tis_spi_request_completed(TPMIf *ti, int ret)
{
    TPMStateSPI *spist = TPM_TIS_SPI(ti);
    TPMState *s = &spist->state;

    /* Inform the common code. */
    tpm_tis_request_completed(s, ret);
}

static enum TPMVersion tpm_tis_spi_get_tpm_version(TPMIf *ti)
{
    TPMStateSPI *spist = TPM_TIS_SPI(ti);
    TPMState *s = &spist->state;

    return tpm_tis_get_tpm_version(s);
}

static void reset_tpm_framing(TPMStateSPI *spist)
{
    spist->in_header = 1;
    spist->write_len = 0;
    //spist->dummy_write_count = 4;
    spist->header_cnt = 0;
    spist->addr = 0;
    //spist->transfer_bytes_left = 0;
    spist->transfer_size = 0;
    spist->tpm_transfer_value = 0;
}

static void tpm_tis_spi_reset(DeviceState *dev)
{
    TPMStateSPI *spist = TPM_TIS_SPI(dev);
    TPMState *s = &spist->state;

    reset_tpm_framing(spist);

    return tpm_tis_reset(s);
}

//static void tpm_tis_spi_ingest_first_header_byte(TPMStateSPI* spist, uint32_t val) {
//
//}

//static int tpm_tis_spi_set_cs(SSIPeripheral *dev, bool select)
//{
//    //fprintf(stderr, "tpm_tis_spi_set_cs: %s\n", select == true ? "true": "false");
//    return 0;
//}

static uint32_t tpm_tis_spi_transfer(SSIPeripheral *dev, uint32_t val)
{
    //fprintf(stderr, "tpm_tis_spi val: 0x%08x\n", val);
    TPMStateSPI *spist = TPM_TIS_SPI(dev);
    if(spist->in_header) {
        if(spist->header_cnt == 0) {
            spist->is_read = val & 0x80 ? 1 : 0;
            spist->transfer_size = val & 0x7f;
            spist->transfer_size += 1;
            spist->write_len = spist->transfer_size + 1; // Set the value in case it's a write.
            ++spist->header_cnt;
            return 0x00;
        } else if (spist->header_cnt == 1) {
            if (val != 0xd4) {
                qemu_log_mask(LOG_GUEST_ERROR, "Received unexpected TIS-SPI opcode, expected 0xd4, got 0x%02x", val);
            }
            ++spist->header_cnt;
            return 0x00;
        } else if (spist->header_cnt == 2) {
            spist->addr = val << 8;
            ++spist->header_cnt;
            return 0x00;
        } else if (spist->header_cnt == 3) {
            spist->addr |= val & 0xff;
            spist->header_cnt = 0;
            spist->in_header = 0;
            if(spist->is_read) {
                if(spist->addr != TPM_DATA_FIFO) {
                    //fprintf(stderr, "Calling tpm_tis_read with addr: 0x%08lx , len : 0x%02x\n", spist->addr, spist->transfer_size);
                    spist->tpm_transfer_value = tpm_tis_read_data(&spist->state, spist->addr, spist->transfer_size);
                    //fprintf(stderr, "tpm_tis_read returned : 0x%04x\n", spist->tpm_transfer_value);
                }
            }
            return 0x01;
        }
    } else {
        --spist->transfer_size;
        uint32_t retval = 0x00;
        if(spist->is_read) {
            if (spist->addr == TPM_DATA_FIFO) {
                retval= tpm_tis_read_data(&spist->state, spist->addr, 1);
            } else {
                retval = (spist->tpm_transfer_value >> ((spist->transfer_size) * 8)) & 0xff;
                //fprintf(stderr, "Dummy read, returning: 0x%02x\n", retval);
            }

        } else {
            if(spist->addr == TPM_DATA_FIFO) {
                tpm_tis_write_data(&spist->state, spist->addr, val & 0xff, 1);
                if(spist->transfer_size == 0) {
                    reset_tpm_framing(spist);
                }
            } else {
                spist->tpm_transfer_value |= ((val & 0xff) << (spist->transfer_size) * 8);
                //fprintf(stderr , "Building value val: 0x%04x, bytes_left -1 : 0x%08x\n", spist->tpm_transfer_value, spist->transfer_size);
                if(spist->transfer_size == 0) {
                    //fprintf(stderr, "Calling tpm_tis_write_data with addr: 0x%08lx value : 0x%08x len: 0x%02x\n", spist->addr, spist->tpm_transfer_value, spist->write_len);
                    tpm_tis_write_data(&spist->state, spist->addr, spist->tpm_transfer_value, spist->write_len);
                    reset_tpm_framing(spist);
                }
            }
        }

        if(spist->transfer_size == 0) {
            reset_tpm_framing(spist);
        }
        return retval;
    }
    return 0xff;
}


static void tpm_tis_spi_realize_ssi(SSIPeripheral *d, Error **errp)
{
    TPMStateSPI *spist = TPM_TIS_SPI(d);
    TPMState *s = &spist->state;

    if (!tpm_find()) {
        error_setg(errp, "at most one TPM device is permitted");
        return;
    }

    /*
     * Get the backend pointer. It is not initialized properly during
     * device_class_set_props
     */
    s->be_driver = qemu_find_tpm_be("tpm0");

    if (!s->be_driver) {
        error_setg(errp, "'tpmdev' property is required");
        return;
    }
}


static void tpm_tis_spi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);
    TPMIfClass *tc = TPM_IF_CLASS(klass);

    device_class_set_legacy_reset(dc, tpm_tis_spi_reset);
    dc->vmsd = &vmstate_tpm_tis_spi;
    device_class_set_props(dc, tpm_tis_spi_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    k->realize = tpm_tis_spi_realize_ssi;
    //k->transfer = 0;//tpm_tis_spi_transfer;
    //k->transfer_raw = tpm_tis_spi_transfer_raw;
    //k->set_cs = tpm_tis_spi_set_cs;
    k->transfer = tpm_tis_spi_transfer;
    tc->model = TPM_MODEL_TPM_TIS;
    tc->request_completed = tpm_tis_spi_request_completed;
    tc->get_version = tpm_tis_spi_get_tpm_version;

}

static const TypeInfo tpm_tis_spi_info = {
    .name = TYPE_TPM_TIS_SPI,
    .parent = TYPE_SSI_PERIPHERAL,
    .instance_size = sizeof(TPMStateSPI),
    .class_init = tpm_tis_spi_class_init,
        .interfaces = (const InterfaceInfo[]) {
        {TYPE_TPM_IF},
        { }
    }
};


static void tpm_tis_spi_register_types(void)
{
    type_register_static(&tpm_tis_spi_info);
}

type_init(tpm_tis_spi_register_types)
