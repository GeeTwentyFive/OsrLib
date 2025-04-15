/*
Copyright (c) 2025 Amar Alic

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _OSR_LIB_H
#define _OSR_LIB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

#include "modules/LZMADec.h"



typedef enum {
        OsrLib__NONE = 0,
        OsrLib__NO_FAIL = 1,
        OsrLib__EASY = 2,
        OsrLib__TOUCH_DEVICE = 4,
        OsrLib__HIDDEN = 8,
        OsrLib__HARD_ROCK = 16,
        OsrLib__SUDDEN_DEATH = 32,
        OsrLib__DOUBLE_TIME = 64,
        OsrLib__RELAX = 128,
        OsrLib__HALF_TIME = 256,
        OsrLib__NIGHT_CORE = 576, // 512, but always used with DT (64) so 576 (512 + 64)
        OsrLib__FLASHLIGHT = 1024,
        OsrLib__AUTOPLAY = 2048,
        OsrLib__SPUN_OUT = 4096,
        OsrLib__AUTO_PILOT = 8192, // Aka. "Relax2"
        OsrLib__PERFECT = 16384,
        OsrLib__KEY4 = 32768,
        OsrLib__KEY5 = 65536,
        OsrLib__KEY6 = 131072,
        OsrLib__KEY7 = 262144,
        OsrLib__KEY8 = 524288,
        OsrLib__KEY_MOD = 1015808, // k4+k5+k6+k7+k8
        OsrLib__FADE_IN = 1048576,
        OsrLib__RANDOM = 2097152,
        OsrLib__CINEMA = 4194304, // Aka. "LastMod"
        OsrLib__TARGET_PRACTICE = 8388608,
        OsrLib__KEY9 = 16777216,
        OsrLib__COOP = 33554432,
        OsrLib__KEY1 = 67108864,
        OsrLib__KEY3 = 134217728,
        OsrLib__KEY2 = 268435456,
        OsrLib__SCORE_V2 = 536870912,
        OsrLib__MIRROR = 1073741824
} OsrLib__Mods;

typedef enum {
        OsrLib__M1 = 1,
        OsrLib__M2 = 2,
        OsrLib__K1 = 4,
        OsrLib__K2 = 8,
        OsrLib__Smoke = 16
} OsrLib__Keys;

typedef struct {

        char mode; // "0" = std, "1" = taiko, "2" = cbt, "3" = mania
        int version;
        char beatmap_hash[32]; // Hex string
        char player_name[256];
        size_t player_name_size;
        char replay_hash[32]; // Hex string
        short three_hundreds;
        short hundreds; // * 150s in taiko
        short fifties; // * small fruit in cbt
        short gekis; // * max 300s in mania
        short katus; // * 200s in mania
        short misses;
        int total_score;
        short greatest_combo;
        char b_perfect_combo;
        OsrLib__Mods mods;
        struct {
                int time;
                float hp; // 0-1
        }* life_bar_graph;
        size_t life_bar_graph_length;
        long long time_stamp;
        struct {
                long long time;
                float x;
                float y;
                OsrLib__Keys keys;
        }* replay_data;
        size_t replay_data_length;
        long long online_score_id;

} OsrLib__Replay;



typedef enum {
        OsrLib__SUCCESS,
        OsrLib__ERROR_FAILED_TO_OPEN_FILE,
        OsrLib__ERROR_FAILED_TO_SEEK_FILE_END,
        OsrLib__ERROR_FAILED_TO_TELL_FILE,
        OsrLib__ERROR_FAILED_TO_SEEK_FILE_START,
        OsrLib__ERROR_FAILED_TO_ALLOCATE_MEMORY,
        OsrLib__ERROR_FAILED_TO_READ_FILE,
        OsrLib__ERROR_FAILED_TO_CLOSE_FILE,
        OsrLib__ERROR_PLAYER_NAME_TOO_LONG,
        OsrLib__ERROR_FAILED_TO_DECOMPRESS_REPLAY_DATA
} OsrLib__Error;



OsrLib__Error OsrLib__Parse(char* path, OsrLib__Replay* out) {

        FILE* replay_file = fopen(path, "rb"); //
        if (replay_file == NULL) return OsrLib__ERROR_FAILED_TO_OPEN_FILE;

        if (fseek(replay_file, 0, SEEK_END) != 0) return OsrLib__ERROR_FAILED_TO_SEEK_FILE_END;
        long replay_file_size = ftell(replay_file); //
        if (replay_file_size == -1L) return OsrLib__ERROR_FAILED_TO_TELL_FILE;
        if (fseek(replay_file, 0, SEEK_SET) != 0) return OsrLib__ERROR_FAILED_TO_SEEK_FILE_START;

        char* replay_file_contents = malloc(replay_file_size); //
        if (replay_file_contents == NULL) return OsrLib__ERROR_FAILED_TO_ALLOCATE_MEMORY;
        if (fread(replay_file_contents, 1, replay_file_size, replay_file) < replay_file_size) return OsrLib__ERROR_FAILED_TO_READ_FILE;

        if (fclose(replay_file) != 0) return OsrLib__ERROR_FAILED_TO_CLOSE_FILE;



        char* i = replay_file_contents;

        out->mode = *i;
        i += 1;

        out->version = *((int*)i);
        i += 4;

        i += 2; // Skip osu! ULEB128-encoded section length (already known to be 32 since MD5 is fixed-length)
        _mm256_storeu_si256(
                (__m256i*)&(out->beatmap_hash),
                _mm256_loadu_si256((__m256i*)i)
        );
        i += 32;

        i += 1; // Skip osu! ULEB128-encoded section length signature (0x0B)
        size_t player_name_length = 0;
        { // Decode ULEB128-encoded length
                size_t shift = 0;
                for (;;) {
                        char b = *i++;
                        player_name_length |= (size_t)(b & 0x7F) << shift;
                        if ((b & 0x80) == 0) break;
                        shift += 7;
                }
        }
        if (player_name_length > 255) return OsrLib__ERROR_PLAYER_NAME_TOO_LONG;
        memcpy(&(out->player_name), i, player_name_length);
        out->player_name_size = player_name_length;
        i += player_name_length;

        i += 2; // Skip osu! ULEB128-encoded section length (already known to be 32 since MD5 is fixed-length)
        _mm256_storeu_si256(
                (__m256i*)&(out->replay_hash),
                _mm256_loadu_si256((__m256i*)i)
        );
        i += 32;

        // three_hundreds(2) + hundreds(2) + fifties(2) + gekis(2) + katus(2) + misses(2) + total_score(4)
        _mm_storeu_si128(
                (__m128i*)&(out->three_hundreds),
                _mm_loadu_si128((__m128i*)i)
        );
        i += 16;

        out->greatest_combo = *((short*)i);
        i += 2;

        out->b_perfect_combo = *i;
        i += 1;

        int mods = *((int*)i); // Used later for checking TP for extra mod info
        out->mods = mods;
        i += 4;

        i += 1; // Skip osu! ULEB128-encoded section length signature (0x0B)
        size_t life_bar_graph_size = 0; // in bytes
        { // Decode ULEB128-encoded length
                size_t shift = 0;
                for (;;) {
                        char b = *i++;
                        life_bar_graph_size |= (size_t)(b & 0x7F) << shift;
                        if ((b & 0x80) == 0) break;
                        shift += 7;
                }
        }
        out->life_bar_graph = malloc(sizeof(*(out->life_bar_graph)) * life_bar_graph_size); // (allocates more than necessary; string representation)
        size_t life_bar_graph_length = 0; // in number of struct array members
        for (char* end = i+life_bar_graph_size; i < end;) {
                out->life_bar_graph[life_bar_graph_length].time = atoi(i);
                while (*i != '|') i++; i++;
                out->life_bar_graph[life_bar_graph_length].hp = atof(i);
                while (*i != ',') i++; i++;
                life_bar_graph_length++;
        }
        out->life_bar_graph_length = life_bar_graph_length;

        out->time_stamp = *((long long*)i);
        i += 8;

        { // Decompress replay data // TODO(?): Optimize with own custom LZMA implementation
                int compressed_data_size = *((int*)i);
                i += 4;
                const unsigned char* compressed_data_lzma_props = i;
                i += 5;
                size_t uncompressed_data_size = *((unsigned long long*)i);
                i += 8;
                char* uncompressed_data = malloc(uncompressed_data_size);
                ELzmaStatus _unused;
                if (LzmaDecode(
                        uncompressed_data,
                        &uncompressed_data_size,
                        i,
                        (SizeT*)&compressed_data_size,
                        compressed_data_lzma_props,
                        5,
                        LZMA_FINISH_ANY,
                        &_unused,
                        &g_Alloc
                ) != SZ_OK) return OsrLib__ERROR_FAILED_TO_DECOMPRESS_REPLAY_DATA;
                out->replay_data = malloc(sizeof(*(out->replay_data)) * uncompressed_data_size); // (allocates more than necessary; string representation)
                size_t replay_data_length = 0; // in number of struct array members
                char* ri = uncompressed_data;
                for (char* end = ri+uncompressed_data_size; ri < end;) {
                        out->replay_data[replay_data_length].time = atoll(ri);
                        while (*ri != '|') ri++; ri++;
                        out->replay_data[replay_data_length].x = atof(ri);
                        while (*ri != '|') ri++; ri++;
                        out->replay_data[replay_data_length].y = atof(ri);
                        while (*ri != '|') ri++; ri++;
                        out->replay_data[replay_data_length].keys = atoi(ri);
                        while (*ri != ',') ri++; ri++;
                        replay_data_length++;
                }
                free(uncompressed_data);
                out->replay_data_length = replay_data_length;
                i += compressed_data_size;
        }

        out->online_score_id = *((long long*)i);
        i += 8;

        // Not adding additional overhead for Target Practice... cause it's Target Practice...
        if (mods & OsrLib__TARGET_PRACTICE) i+= 8;

        // TODO(?): Lazer ScoreInfo



        free(replay_file_contents);

        return OsrLib__SUCCESS;

}

void OsrLib__Free(OsrLib__Replay* replay) {
        free(replay->life_bar_graph);
        free(replay->replay_data);
}



#endif // _OSR_LIB_H