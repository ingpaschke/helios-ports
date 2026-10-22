/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <curses.h>
#include <menu.h>
#include <panel.h>
#include <form.h>

int main(void)
{
    WINDOW *window;
    PANEL *panel;
    MENU *menu;
    FORM *form;
    ITEM *items[2] = {NULL, NULL};
    FIELD *fields[2] = {NULL, NULL};
    int failures = 0;
    if (!initscr()) return 2;
    window = newwin(4, 24, 1, 1);
    panel = window ? new_panel(window) : NULL;
    if (!panel) ++failures;
    else {
        if (hide_panel(panel) == ERR || show_panel(panel) == ERR) ++failures;
        update_panels();
        if (del_panel(panel) == ERR) ++failures;
    }
    items[0] = new_item("first", "menu item");
    menu = items[0] ? new_menu(items) : NULL;
    if (!menu) ++failures;
    else {
        if (post_menu(menu) == ERR || unpost_menu(menu) == ERR) ++failures;
        if (free_menu(menu) == ERR) ++failures;
    }
    if (items[0] && free_item(items[0]) == ERR) ++failures;
    fields[0] = new_field(1, 16, 1, 1, 0, 0);
    form = fields[0] ? new_form(fields) : NULL;
    if (!form) ++failures;
    else {
        if (set_field_buffer(fields[0], 0, "Helios") == ERR ||
            post_form(form) == ERR || unpost_form(form) == ERR) ++failures;
        if (free_form(form) == ERR) ++failures;
    }
    if (fields[0] && free_field(fields[0]) == ERR) ++failures;
    if (window && delwin(window) == ERR) ++failures;
    endwin();
    printf("%s: panel, menu and form (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}
