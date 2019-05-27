#ifndef TRACKSTRUCTS_H_
#define TRACKSTRUCTS_H_
#include <stdint.h>
struct VGMHeader
{
    uint32_t indent;
    uint32_t EoF;
    uint32_t version;
    uint32_t sn76489Clock;
    uint32_t ym2413Clock;
    uint32_t gd3Offset;
    uint32_t totalSamples;
    uint32_t loopOffset;
    uint32_t loopNumSamples;
    uint32_t rate;
    uint32_t snX;
    uint32_t ym2612Clock;
    uint32_t ym2151Clock;
    uint32_t vgmDataOffset;
    uint32_t segaPCMClock;
    uint32_t spcmInterface;
    uint32_t rf5C68clock;
    uint32_t ym2203clock;
    uint32_t ym2608clock;
    uint32_t ym2610clock;
    uint32_t ym3812clock;
    uint32_t ym3526clock;
    uint32_t y8950clock;
    uint32_t ymf262clock;
    uint32_t ymf271clock;
    uint32_t ymz280Bclock;
    uint32_t rf5C164clock;
    uint32_t pwmclock;
    uint32_t ay8910clock;
    uint32_t ayclockflags;
    uint32_t vmlblm;
    uint32_t gbdgmclock;
    uint32_t nesapuclock;
    uint32_t multipcmclock;
    uint32_t upd7759clock;
    uint32_t okim6258clock;
    uint32_t ofkfcf;
    uint32_t okim6295clock;
    uint32_t k051649clock;
    uint32_t k054539clock;
    uint32_t huc6280clock;
    uint32_t c140clock;
    uint32_t k053260clock;
    uint32_t pokeyclock;
    uint32_t qsoundclock;
    uint32_t scspclock;
    uint32_t extrahdrofs;
    uint32_t wonderswanclock;
    uint32_t vsuClock;
    uint32_t saa1099clock;

    void Reset()
    {
        indent = 0;
        EoF = 0;
        version = 0;
        sn76489Clock = 0;
        ym2413Clock = 0;
        gd3Offset = 0;
        totalSamples = 0;
        loopOffset = 0;
        loopNumSamples = 0;
        rate = 0;
        snX = 0;
        ym2612Clock = 0;
        ym2151Clock = 0;
        vgmDataOffset = 0;
        segaPCMClock = 0;
        spcmInterface = 0;
        rf5C68clock = 0;
        ym2203clock = 0;
        ym2608clock = 0;
        ym2610clock = 0;
        ym3812clock = 0;
        ym3526clock = 0;
        y8950clock = 0;
        ymf262clock = 0;
        ymf271clock = 0;
        ymz280Bclock = 0;
        rf5C164clock = 0;
        pwmclock = 0;
        ay8910clock = 0;
        ayclockflags = 0;
        vmlblm = 0;
        gbdgmclock = 0;
        nesapuclock = 0;
        multipcmclock = 0;
        upd7759clock = 0;
        okim6258clock = 0;
        ofkfcf = 0;
        okim6295clock = 0;
        k051649clock = 0;
        k054539clock = 0;
        huc6280clock = 0;
        c140clock = 0;
        k053260clock = 0;
        pokeyclock = 0;
        qsoundclock = 0;
        scspclock = 0;
        extrahdrofs = 0;
        wonderswanclock = 0;
        vsuClock = 0;
        saa1099clock = 0;
    }
};

struct GD3
{
    uint32_t size;
    String enTrackName;
    String enGameName;
    String enSystemName;
    String enAuthor;
    String releaseDate;
    void Reset()
    {
        size = 0;
        enTrackName = "";
        enGameName = "";
        enSystemName = "";
        enAuthor = "";
        releaseDate = "";
    }
};

enum FileStrategy {FIRST_START, NEXT, PREV, RND, REQUEST};
enum PlayMode {LOOP, PAUSE, SHUFFLE, IN_ORDER};
static VGMHeader header;
static GD3 gd3;
#endif