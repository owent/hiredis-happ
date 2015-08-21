//
// Created by OWenT on 2015/08/19.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_DETAIL_CRC16_H
#define HIREDIS_HAPP_HIREDIS_HAPP_DETAIL_CRC16_H

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    uint16_t crc16(const char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif