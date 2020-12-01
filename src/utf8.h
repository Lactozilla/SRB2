#ifndef _UTF8_H_
#define _UTF8_H_

#include <stdarg.h>

#include "doomtype.h"

#define u_int32_t UINT32

/* is c the start of a utf8 sequence? */
#define isutf(c) (((c)&0xC0)!=0x80)

/* convert UTF-8 data to wide character */
int u8_toucs(u_int32_t *dest, int sz, const char *src, int srcsz);

/* the opposite conversion */
int u8_toutf8(char *dest, int sz, u_int32_t *src, int srcsz);

/* single character to UTF-8 */
int u8_wc_toutf8(char *dest, u_int32_t ch);

/* character number to byte offset */
int u8_offset(char *str, int charnum);

/* byte offset to character number */
int u8_charnum(char *s, int offset);

/* return next character, updating an index variable */
u_int32_t u8_nextchar(const char *s, int *i);

/* move to next character */
void u8_inc(char *s, int *i);

/* move to previous character */
void u8_dec(char *s, int *i);

/* returns length of next utf-8 sequence */
int u8_seqlen(const char *s);

/* return a pointer to the first occurrence of ch in s, or NULL if not
   found. character index of found character returned in *charn. */
const char *u8_strchr(const char *s, u_int32_t ch, int *charn);

/* same as the above, but searches a buffer of a given size instead of
   a NUL-terminated string. */
char *u8_memchr(char *s, u_int32_t ch, size_t sz, int *charn);

/* count the number of characters in a UTF-8 string */
int u8_strlen(const char *s);

#endif // _UTF8_H_
