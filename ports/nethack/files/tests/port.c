/* Regression checks for real port code, without booting Helios. */
#include "hack.h"
#include <assert.h>

char plname[PL_NSIZ];
static struct tm test_date;
struct tm *localtime(const time_t *unused) { return &test_date; }

int main(void)
{
    static int years[] = { 99, 100, 101, 126, 199, 200 };
    static const char *expected[] = {
        "991009", "001009", "011009", "261009", "991009", "001009"
    };
    char filename[40] = "a.long/player name";
    unsigned int i;
    for (i = 0; i < sizeof(years)/sizeof(years[0]); ++i) {
        test_date.tm_year = years[i];
        test_date.tm_mon = 9;
        test_date.tm_mday = 9;
        assert(strcmp(get_date(), expected[i]) == 0);
    }
    regularize(filename);
    assert(strlen(filename) <= 8);
    assert(!strchr(filename, '.') && !strchr(filename, '/') && !strchr(filename, ' '));
    strcpy(plname, "LongPlayerName");
    set_savefile_name();
    assert(strncmp(SAVEF, "save/", 5) == 0);
    assert(strlen(SAVEF + 5) <= 8);
    puts("PASS: century dates and DOS save/level filenames");
    return 0;
}
