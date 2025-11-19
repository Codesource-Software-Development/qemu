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
    //SSIBus *bus;
} TPMStateSPI;

//DECLARE_INSTANCE_CHECKER(TPMStateSPI, TPM_TIS_SPI, TYPE_TPM_TIS_SPI)

typedef struct TPMTisSpiClass {
    SSIPeripheralClass parent_class;
    DeviceRealize parent_realize;
    //void (*parent_realize)(SSIPeripheral *dev, Error **errp);
} TPMTisSpiClass;

OBJECT_DECLARE_TYPE(TPMStateSPI, TPMTisSpiClass, TPM_TIS_SPI)

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

#if 0
static void tpm_tis_spi_realizefn(DeviceState *dev, Error **errp)
{
    TPMStateSPI *spist = TPM_TIS_SPI(dev);
    TPMState *s = &spist->state;
#if 0
    SSIPeripheral *ssip = SSI_PERIPHERAL(dev);

    SSIPeripheralClass *ssc = SSI_PERIPHERAL_GET_CLASS(ssip);

    TPMTisSpiClass *ttc = TPM_TIS_SPI_GET_CLASS(dev);

    if(ttc->parent_realize) {
        ttc->parent_realize(dev, errp);
        if(*errp) {
            return;
        }
    }

    if(ssc->realize) {
        ssc->realize(ssip, errp);
    }
#endif

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
#endif

static void tpm_tis_spi_reset(DeviceState *dev)
{
    TPMStateSPI *spist = TPM_TIS_SPI(dev);
    TPMState *s = &spist->state;

    //tpm_tis_spi_clear_data(spist);


    //spist->

    return tpm_tis_reset(s);
}

static uint32_t tpm_tis_spi_transfer(SSIPeripheral *dev, uint32_t val)
{
    fprintf(stderr, "[TPM-SPI] transfer val=0x%02x\n", val);
    return 0xFF;
}

static uint32_t tpm_tis_spi_transfer_raw(SSIPeripheral *dev, uint32_t val)
{
    fprintf(stderr, "[TPM-SPI] transfer val=0x%02x\n", val);
    return 0xFF;
}


static void tpm_tis_spi_realize_ssi(SSIPeripheral *d, Error **errp)
{
    TPMStateSPI *spist = TPM_TIS_SPI(d);
    TPMState *s = &spist->state;
    //SSIPeripheralClass *ssic = SSI_PERIPHERAL_GET_CLASS(s);
    //d->spc = ssic;
    //(void) ssic;

    //TPMStateSPI *spist = TPM_TIS_SPI(dev);
    //TPMState *s = &spist->state;

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
    //TPMTisSpiClass *ttc = TPM_TIS_SPI_CLASS(klass);
    //PMTisSpiClass *ttc = TPM_TIS_SPI(klass);

    //ttc->parent_realize = dc->realize;
    //dc->realize = tpm_tis_spi_realizefn;
    device_class_set_legacy_reset(dc, tpm_tis_spi_reset);
    dc->vmsd = &vmstate_tpm_tis_spi;
    device_class_set_props(dc, tpm_tis_spi_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    /* @TODO Implement SSIPeripheralClass initialization here */
    //(void) k;
    k->realize = tpm_tis_spi_realize_ssi;
    k->transfer = tpm_tis_spi_transfer;
    k->transfer_raw = tpm_tis_spi_transfer_raw;

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
