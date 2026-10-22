/* Emit target compile-time assertions from the host data generator ABI. */
#include "hack.h"
#include "sp_lev.h"
#include "dgn_file.h"
#include "qtext.h"
#include <stddef.h>
#define SIZE_ASSERT(t) printf("typedef char size_%d[(sizeof(" #t ") == %u) ? 1 : -1];\n", ++n, (unsigned)sizeof(t))
#define OFFSET_ASSERT(t, f) printf("typedef char off_%d[(offsetof(" #t ", " #f ") == %u) ? 1 : -1];\n", ++n, (unsigned)offsetof(t, f))
int main(void)
{
    int n = 0;
    puts("#include \"hack.h\"\n#include \"sp_lev.h\"\n#include \"dgn_file.h\"\n#include \"qtext.h\"\n#include <stddef.h>");
    SIZE_ASSERT(char); SIZE_ASSERT(short); SIZE_ASSERT(int); SIZE_ASSERT(long);
    SIZE_ASSERT(void *); SIZE_ASSERT(boolean); SIZE_ASSERT(aligntyp);
    SIZE_ASSERT(lev_init); SIZE_ASSERT(door); SIZE_ASSERT(room_door);
    SIZE_ASSERT(trap); SIZE_ASSERT(monster); SIZE_ASSERT(object);
    SIZE_ASSERT(altar); SIZE_ASSERT(drawbridge); SIZE_ASSERT(walk);
    SIZE_ASSERT(digpos); SIZE_ASSERT(lad); SIZE_ASSERT(stair);
    SIZE_ASSERT(region); SIZE_ASSERT(lev_region); SIZE_ASSERT(gold);
    SIZE_ASSERT(engraving); SIZE_ASSERT(fountain); SIZE_ASSERT(sink);
    SIZE_ASSERT(pool); SIZE_ASSERT(corridor);
    OFFSET_ASSERT(corridor, dest);
    OFFSET_ASSERT(lev_region, delarea);
    SIZE_ASSERT(struct tmpdungeon); SIZE_ASSERT(struct tmplevel);
    SIZE_ASSERT(struct tmpbranch); SIZE_ASSERT(struct qtmsg);
    OFFSET_ASSERT(monster, id); OFFSET_ASSERT(monster, name);
    OFFSET_ASSERT(monster, appear_as); OFFSET_ASSERT(object, corpsenm);
    OFFSET_ASSERT(object, name); OFFSET_ASSERT(struct qtmsg, offset);
    OFFSET_ASSERT(struct qtmsg, size); OFFSET_ASSERT(struct tmpdungeon, boneschar);
    OFFSET_ASSERT(gold, amount); OFFSET_ASSERT(engraving, e);
    OFFSET_ASSERT(engraving, etype); OFFSET_ASSERT(struct tmpdungeon, flags);
    OFFSET_ASSERT(struct tmplevel, chance); OFFSET_ASSERT(struct tmpbranch, chain);
    return 0;
}
