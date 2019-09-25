/*
  A18_Decrypt - Gust (Koei/Tecmo) archive decrypter/unpacker for PC games
  Copyright © 2019 VitaSmith
  Copyright © 2018 Yuri Hime, shizukachan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

struct fileHeader
{
    unsigned int unknown1;
    unsigned int fileEntries;
    unsigned int unknown2;
    unsigned int unknown3;
} typedef fileHeader;

struct fileEntry    //len = 0xA8
{
    unsigned char filename[128];
    unsigned int length;
    unsigned char key[20];
    unsigned __int64 data_offset;   //only change from A17
    unsigned __int64 dummy;         //and an extra padding byte. Why, I don't know.
} typedef fileEntry;

static fileHeader Header;
static fileEntry* Entries = NULL;

bool CreatePath(char* wsPath)       //ripped off the web. don't judge me
{
    DWORD attr;
    int pos;
    bool result = true;
    // Look for existing object:
    attr = GetFileAttributesA((LPCSTR)wsPath);
    if (0xFFFFFFFF == attr)         // doesn't exist yet - create it!
    {
        pos = 0;
        for (int n = strlen(wsPath); n > 0; n--)
            if (wsPath[n] == '\\') {
                pos = n;
                break;
            }
        if (0 < pos) {
            // Create parent dirs:
            wsPath[pos] = 0;
            char* newPath = (char*)malloc(sizeof(char) * (strlen(wsPath) + 1));
            strcpy(newPath, wsPath);
            result = CreatePath(newPath);
            free(newPath);
            wsPath[pos] = '\\';
        }
        // Create node:
        result = result && CreateDirectoryA((LPCSTR)wsPath, NULL);
    } else if (FILE_ATTRIBUTE_DIRECTORY != attr) {  // object already exists, but is not a dir
        SetLastError(ERROR_FILE_EXISTS);
        result = false;
    }

    return result;
}

void decode(unsigned char* a, unsigned char* k, unsigned int length)
{//a17 has a SSE2 optimized version. This one is done bytewise because I'm lazy.
    for (int i = 0; i < length; i++)    // Dear Japan,
        a[i] = a[i] ^ k[i % 20];        // It would be nice if you actually used some kind of fun encryption instead of this silly crap.
}

unsigned char blank_key[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("A18_decrypt by Yuri Hime\nUsage: %s <Atelier PAK file>\n\nDumps the Atelier PAK format archive to the current directory.\nIf unpacked to the Atelier Sophie directory, you can remove the PACK00.pak file\nand it will use the unpacked assets. Have fun, modders!", argv[0]);
        return 0;
    }
    FILE* A = fopen(argv[1], "rb");
    if (A == NULL) {
        fprintf(stderr, "Can't open PAK file");
        return -1;
    }
    //#define DECODE_PAK 1
#ifdef DECODE_PAK
    //to use this feature, copy the encrypted PACK00.pak file to ./PACK00.dec
    //It will re-create the PACK00.pak file, except all data will be unencrypted, into ./PACK00.dec
    //The game engine should accept ./PACK00.dec as equivalent to PACK00.pak.
    FILE* C = fopen("PACK01.dec", "r+b");
    //used to see if the engine cares about unencrypted data... it doesn't.
#endif
    if (fread(&Header, sizeof(Header), 1, A) == 0) {
        fprintf(stderr, "can't read header");
        return -1;
    }
#ifdef DECODE_PAK
    fwrite(&Header, sizeof(Header), 1, C);
#endif
    if ((Header.unknown1 != 0x20000) || (Header.unknown2 != 0x10) || (Header.unknown3 != 0x0D)) {
        //		fprintf(stderr,"WARNING: signature of file doesn't match Atelier Sophie archive.\n");
        fprintf(stderr, "WARNING: signature of file doesn't match Atelier Firis archive.\n");
    }
    if (Header.fileEntries > 16384) {
        fprintf(stderr, "WARNING: fileEntries over 16384, is this a supported archive?\n");
    }
    Entries = (fileEntry*)malloc(sizeof(fileEntry) * Header.fileEntries);
    fread(Entries, sizeof(fileEntry), Header.fileEntries, A);
    __int64 file_data_offset = _ftelli64(A);
    puts("OFFSET    SIZE     NAME");
    char path[256];
    unsigned char* buf;
    unsigned char skip_decode;
    for (int i = 0; i < Header.fileEntries; i++) {
        skip_decode = 1;
        for (int j = 0; j < 20; j++)
            if (Entries[i].key[j] != blank_key[j])
                skip_decode = 0;
        if (!skip_decode)
            decode(Entries[i].filename, Entries[i].key, 128);
        printf("%09I64x %08x %s\n", Entries[i].data_offset + file_data_offset, Entries[i].length, Entries[i].filename);
        strcpy(path, (char*)Entries[i].filename + 1);
        for (int n = strlen(path); n > 0; n--) {
            if (path[n] == '\\') {
                path[n] = 0;
                break;
            }
        }
        if (CreatePath(path) == false) {
            fprintf(stderr, "Can't create path %s\n", path);
            continue;
        }
        FILE* B = NULL;
        B = fopen((char*)(Entries[i].filename + 1), "wb");
        if (B == NULL) {
            fprintf(stderr, "Can't open file %s\n", Entries[i].filename + 1);
            continue;
        }
        _fseeki64(A, Entries[i].data_offset + file_data_offset, SEEK_SET);
#ifdef DECODE_PAK
        _fseeki64(C, Entries[i].data_offset + file_data_offset, SEEK_SET);
#endif
        buf = (unsigned char*)malloc(sizeof(unsigned char) * Entries[i].length);
        fread(buf, 1, Entries[i].length, A);
        if (!skip_decode)
            decode(buf, Entries[i].key, Entries[i].length);
        fwrite(buf, 1, Entries[i].length, B);
#ifdef DECODE_PAK
        fwrite(buf, 1, Entries[i].length, C);
#endif
        free(buf);
        fclose(B);
    }
#ifdef DECODE_PAK
    _fseeki64(C, 0x10, SEEK_SET);
    for (int i = 0; i < Header.fileEntries; i++) {
        memset(Entries[i].key, 0, sizeof(Entries[i].key));
        fwrite(&Entries[i], sizeof(fileEntry), 1, C);
    }
    fclose(C);
#endif
    fclose(A);
    free(Entries);
    return 0;
}
