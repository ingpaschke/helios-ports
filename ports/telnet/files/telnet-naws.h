/* RFC 1073 network byte order and RFC 854 IAC escaping. Max packet: 13 bytes. */
static int ktm_naws(unsigned char *out, unsigned int cols, unsigned int rows)
{
    unsigned char size[4];
    int i, n=0;
    size[0]=cols>>8; size[1]=cols; size[2]=rows>>8; size[3]=rows;
    out[n++]=255; out[n++]=250; out[n++]=31;
    for (i=0;i<4;i++) {
        out[n++]=size[i];
        if (size[i]==255) out[n++]=255;
    }
    out[n++]=255; out[n++]=240;
    return n;
}
