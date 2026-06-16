#pragma once

#include <inttypes.h>

#define IMG_FMT_FILE_EXTENSION  ".BIN"
#define IMG_FMT_HEADER_MAGIC    "PTG"

constexpr uint16_t kImgFmtMagicHeaderLength           = 3U;

struct __attribute__((packed)) ImgFmtHeader
{
    char    magic[3];
    uint8_t flags;
    uint8_t reserved[12];
};

constexpr uint16_t kImgFmtHeaderLength                = sizeof(ImgFmtHeader);
