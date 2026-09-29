#include <string.h>
#include "utf8.h"

// Calculates the length of a utf-8 encoded string
int utf8_length(const char *string)
{
    int length = 0;
    char *ptr = (char*) string;
    while (*ptr != '\0') {
        // If byte is 0xxxxxxx, then it's a 1 byte (ASCII) char
        if ((*ptr & 0x80) == 0)
            ptr++;

        // If byte is 110xxxxx, then it's a 2 byte char
        else if ((*ptr & 0xE0) == 0xC0)
            ptr +=2;

        // If byte is 1110xxxx, then it's a 3 byte char
        else if ((*ptr & 0xF0) == 0xE0)
            ptr +=3;

        // If byte is 11110xxx, then it's a 4 byte char
        else if ((*ptr & 0xF8) == 0xF0)
            ptr+=4;

    length++;
    }
    return length;
}

// A function to truncate a utf-8 encoded string to max number of pixels. It never walks back
// past the start of the string: when fewer than three characters fit, the whole title becomes
// "...", and a title of one or two characters is left as it is.
void utf8_truncate(char *string, int width, int max_width)
{
    int string_length = utf8_length(string);
    if (string_length == 0)
        return;
    int avg_width = width / string_length;
    if (avg_width < 1)
        avg_width = 1;
    int num_chars = max_width / avg_width;
    int spaces = (string_length - num_chars) + 3; // Number of spaces to go back
    if (spaces > string_length)
        spaces = string_length;
    char *ptr = string + strlen(string); // Change to null character of string
    int chars = 0;

    // Go back required number of spaces
    do {
        ptr--;
        if (!(*ptr & 0x80)) // ASCII characters have 0 as most significant bit
            chars++;
        else { // Non-ASCII character detected
            do {
                ptr--;
            } while (ptr > string && (*ptr & 0xC0) == 0x80); // Non-ASCII most significant byte begins with 0b11
            chars++;
        }
    } while (chars < spaces && ptr > string);

    // Add "..." to end of string to inform user of truncation
    if (strlen(ptr) > 2) {
        *ptr = '.';
        *(ptr + 1) = '.';
        *(ptr + 2) = '.';
        *(ptr + 3) = '\0';
    }
}

// A function to shorten a string utf8_truncate has cut, by the character before its "...". It
// returns 0 when there is none left to remove, or the string was not cut.
int utf8_shorten(char *string)
{
    size_t length = strlen(string);
    if (length <= 3 || strcmp(string + length - 3, "...") != 0)
        return 0;
    char *dots = string + length - 3;
    char *ptr = dots - 1;
    while (ptr > string && (*ptr & 0xC0) == 0x80) // Back to the first byte of the character
        ptr--;
    memmove(ptr, dots, 4);
    return 1;
}
