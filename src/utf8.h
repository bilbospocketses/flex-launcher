// UTF-8 string helpers. Pure: no SDL, so tests/test_utf8.c can build them on their own.
#ifndef UTF8_H
#define UTF8_H

int utf8_length(const char *string);
void utf8_truncate(char *string, int width, int max_width);

#endif
