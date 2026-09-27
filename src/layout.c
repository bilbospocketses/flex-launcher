#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "layout.h"

// A function to pick each setting from the menu, else from [Layout], else the built-in default
LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin)
{
    LayoutOverrides result;
    result.rows = menu.rows ? menu.rows : (global.rows ? global.rows : builtin.rows);
    result.columns = menu.columns ? menu.columns : (global.columns ? global.columns : builtin.columns);
    result.icon_cap = menu.icon_cap ? menu.icon_cap : (global.icon_cap ? global.icon_cap : builtin.icon_cap);
    return result;
}

// A function to read a positive whole number, rejecting anything else ("3x", "-1", "0", "")
bool layout_parse_count(const char *value, int *count)
{
    if (value == NULL || value[0] == '\0' || strlen(value) > 6)
        return false;
    for (const char *p = value; *p != '\0'; p++) {
        if (!isdigit((unsigned char) *p))
            return false;
    }
    int number = atoi(value);
    if (number <= 0)
        return false;
    *count = number;
    return true;
}
