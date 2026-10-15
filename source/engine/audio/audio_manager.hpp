#pragma once

// https://github.com/devkitPro/3ds-examples/blob/master/audio/ogg-vorbis-decoding/source/main.c

#include <tremor/ivorbisfile.h>
#include <tremor/ivorbiscodec.h>
#include <3ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

namespace {
const char *vorbisStrError(int error) {
    switch(error) {
        case OV_FALSE:
            return "OV_FALSE: A request did not succeed.";
        case OV_HOLE:
            return "OV_HOLE: There was a hole in the page sequence numbers.";
        case OV_EREAD:
            return "OV_EREAD: An underlying read, seek or tell operation failed.";
        case OV_EFAULT:
            return "OV_EFAULT: A NULL pointer was passed where none was expected, or an internal library error was encountered.";
        case OV_EIMPL:
            return "OV_EIMPL: The stream used a feature which is not implemented.";
        case OV_EINVAL:
            return "OV_EINVAL: One or more parameters to a function were invalid.";
        case OV_ENOTVORBIS:
            return "OV_ENOTVORBIS: This is not a valid Ogg Vorbis stream.";
        case OV_EBADHEADER:
            return "OV_EBADHEADER: A required header packet was not properly formatted.";
        case OV_EVERSION:
            return "OV_EVERSION: The ID header contained an unrecognised version number.";
        case OV_EBADPACKET:
            return "OV_EBADPACKET: An audio packet failed to decode properly.";
        case OV_EBADLINK:
            return "OV_EBADLINK: We failed to find data we had seen before or the stream was sufficiently corrupt that seeking is impossible.";
        case OV_ENOSEEK:
            return "OV_ENOSEEK: An operation that requires seeking was requested on an unseekable stream.";
        default:
            return "Unknown error.";
    }
}
}

static const int THREAD_AFFINITY = -1;
static const int THREAD_STACK_SZ = 32 * 1024;

ndspWaveBuf s_waveBufs[3];
int16_t *s_audioBuffer = NULL;

LightEvent s_event;
volatile bool s_quit = false;

class AudioManager {
    AudioManager get() {
        static AudioManager instance;
        return instance;
    }

    AudioManager initialize() {
        ndspInit();

        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    }

    AudioManager createStream(std::string path) {
#ifdef __3DS__
        path = "romfs:/assets/" + path;
#endif


    }

    AudioManager staticSound(std::string path) {
#ifdef __3DS__
        path = "romfs:/assets/" + path;
#endif
    }
}