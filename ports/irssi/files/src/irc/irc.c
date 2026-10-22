void irc_core_init(void);
void irc_core_deinit(void);
void irc_dcc_init(void);
void irc_dcc_deinit(void);
void irc_notifylist_init(void);
void irc_notifylist_deinit(void);
void irc_init(void) {irc_core_init();irc_dcc_init();irc_notifylist_init();}

void irc_deinit(void) {irc_core_deinit();irc_dcc_deinit();irc_notifylist_deinit();}
