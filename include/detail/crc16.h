//
// Created by OWenT on 2015/08/19.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_DETAIL_CRC16_H
#define HIREDIS_HAPP_HIREDIS_HAPP_DETAIL_CRC16_H

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hiredis {
    namespace happ {
        uint16_t crc16(const char *buf, size_t len);
    }
} // namespace hiredis

#endif