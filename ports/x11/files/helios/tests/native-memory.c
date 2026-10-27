/* Check native shared widget data and VT-only input without another login. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
/* Helios syslib and Xt both typedef Object. */
#define Object HeliosObject
#include <termios.h>
#undef Object
#include <X11/IntrinsicP.h>
#include <X11/Xaw/AsciiText.h>

int main(int argc, char **argv)
{
    WidgetClass widget = asciiTextWidgetClass;
    if (argc > 1 && !strcmp(argv[1], "vt")) {
        static char text[] = "\033[?38hVT_ONLY_OK\n";
        struct termios saved, raw;
        unsigned offset = 0;
        int count, failed = 0;
        if (tcgetattr(1, &saved)) return 1;
        raw = saved;
        /* Bypass ttyserv's output translation to test xterm's parser. */
        raw.c_oflag &= ~OPOST;
        if (tcsetattr(1, TCSANOW, &raw)) return 1;
        while (offset < sizeof(text)-1) {
            count = write(1, text+offset, sizeof(text)-1-offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { failed = 1; break; }
            offset += count;
        }
        if (tcsetattr(1, TCSANOW, &saved)) failed = 1;
        return failed;
    }
    /* Run while another Xt application is alive: its initialized classes
     * must not affect this task's class records in the shared library. */
    if (widget->core_class.class_inited) {
        fputs("widget state was shared between tasks\n", stderr);
        return 1;
    }
    XtToolkitInitialize();
    XtInitializeWidgetClass(widget);
    if (!widget->core_class.class_inited ||
        widget->core_class.realize == XtInheritRealize ||
        widget->core_class.resize == XtInheritResize ||
        widget->core_class.expose == XtInheritExpose) {
        fputs("unresolved widget inheritance\n", stderr);
        return 1;
    }
    puts("PRIVATE_WIDGET_DATA_OK CANONICAL_FUNCTION_POINTERS_OK");
    return 0;
}
