#include <string.h>
#include "command.h"

int helios_command_split(char *text, char **args, unsigned capacity)
{
    char *src=text,*dst=text;
    unsigned count=0;
    if (!capacity) return -1;
    while (*src) {
        char quote=0;
        while (*src==' ' || *src=='\t') src++;
        if (!*src) break;
        if (count+1>=capacity) return -1;
        args[count++]=dst;
        while (*src && (quote || (*src!=' ' && *src!='\t'))) {
            char c=*src++;
            if (quote=='\'') {
                if (c=='\'') quote=0;
                else *dst++=c;
            } else if (c=='\'' && !quote) quote=c;
            else if (c=='"') quote=quote?0:c;
            else {
                /* Leave expansion, quoting escapes and shell operators to
                 * the real shell. Operators inside quotes are literals. */
                if (c=='\\' || c=='$' || c=='`' || c=='\n' || c=='\r' ||
                    (!quote && strchr("|&;<>()*?[]{}~#!",c))) return -1;
                *dst++=c;
            }
        }
        if (quote) return -1;
        /* Skip the separator before overwriting it when src==dst. */
        while (*src==' ' || *src=='\t') src++;
        *dst++=0;
    }
    args[count]=0;
    return count;
}
