/* SPDX-License-Identifier: MIT
 * Native Helios terminal servers translate keys to C1 sequences even in
 * raw mode. Accept those alongside the terminal's terminfo sequences.
 * Match servers/ttyserv/ttyserv/globals.c; do not alter eight-bit text.
 */
static void helios_keys(SCREEN *sp)
{
    static const struct { const char *sequence; unsigned key; } keys[] = {
        {"\233A", KEY_UP}, {"\233B", KEY_DOWN},
        {"\233C", KEY_RIGHT}, {"\233D", KEY_LEFT},
        {"\233T~", KEY_SR}, {"\233S~", KEY_SF},
        {"\233 @~", KEY_SRIGHT}, {"\233 A~", KEY_SLEFT},
        {"\233?~", KEY_HELP}, {"\2331z", KEY_UNDO},
        {"\233H", KEY_HOME}, {"\2332z", KEY_END},
        {"\2333z", KEY_PPAGE}, {"\2334z", KEY_NPAGE},
        {"\233@", KEY_IC}
    };
    unsigned n;
    for (n = 0; n < sizeof(keys) / sizeof(keys[0]); ++n)
        (void) _nc_add_to_try(&sp->_keytry, keys[n].sequence, keys[n].key);
    for (n = 0; n < 40; ++n) {
        char sequence[6], *p = sequence;
        *p++ = '\233';
        if (n >= 10) *p++ = (char) ('0' + n / 10);
        *p++ = (char) ('0' + n % 10);
        *p++ = '~';
        *p = 0;
        (void) _nc_add_to_try(&sp->_keytry, sequence, KEY_F(n + 1));
    }
}
