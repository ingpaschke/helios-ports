/* SPDX-License-Identifier: MIT */
/* Helios currently supplies the C locale, without message catalogues. */
char *dgettext(const char *domain, const char *message)
{
    (void)domain;
    return (char *)message;
}

char *gettext(const char *message)
{
    return (char *)message;
}

char *bindtextdomain(const char *domain, const char *directory)
{
    (void)domain;
    (void)directory;
    return "/helios/local/openwin/lib/locale";
}

char *textdomain(const char *domain)
{
    (void)domain;
    return "SUNW_WST_LIBXVIEW";
}
