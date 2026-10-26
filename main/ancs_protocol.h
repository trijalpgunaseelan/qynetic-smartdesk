#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t uid;
    char app[64], title[64], message[128], positive[32], negative[32];
} sd_ancs_attributes;
typedef struct {
    sd_ancs_attributes value;
    uint32_t expected_uid;
    uint16_t length, offset;
    uint8_t phase, header_bytes, attribute_index, id;
    bool complete, failed;
} sd_ancs_parser;
void sd_ancs_parser_init(sd_ancs_parser *parser, uint32_t uid);
/* 1: all requested tuples received; 0: more fragments needed; -1: invalid. */
int sd_ancs_parser_feed(sd_ancs_parser *parser, const uint8_t *bytes, size_t size);
size_t sd_ancs_request(uint32_t uid, uint8_t out[14]);
