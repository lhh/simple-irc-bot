#include <string.h>
#include <malloc.h>
#include "irc.h"

/*
 * If buf == NULL:
 * Returns single-malloc'd array of pointers to
 * strings.  Caller must free.
 *
 * char **ret =
 *    (char *)(ret + p),
 *    (char *)(ret + q),
 *    (char *)(ret + ...),
 *    (char *)(ret + z),
 *    NULL,
 *    "abc",    (= ret + p)
 *    "def",    (= ret + q)
 *    ...
 *    "zzz"     (= ret + z]
 */
char **
strsplit2(const char *line, const char *chars, void *buf, size_t bufsz)
{
	char **ret = NULL;
	char *p;
	int c, x = 0, l = strlen(line), word = 0;

	/*
	 * Count the number of "words"
	 */
	for (c = 0; c < l; c++) {
		if (strchr(chars, line[c])) {
			word = 0;
			continue;
		}
		if (word == 0) {
			word = 1;
			++x;
		}
	}

	/*
	 * Set up our return buffer:
	 * count of splits (for NULL) +
	 * length of our input + 1 (trailing 0)
	 */
	word = l + 1 + sizeof(char *) * (x+1);
	if (buf && bufsz) {
		if (bufsz < word) {
			/* Return buffer provided is too small */
			return (void *)-1;
		}
		ret = (char **)buf;
	} else {
		ret = malloc(word);
		if (ret == NULL) {
			/* Return buffer provided is too small */
			return (void *)-1;
		}
	}

	memset(ret, 0, word);
	word = 0;

	p = (char *)(&ret[x+1]);
	memcpy((char *)p, line, l);
	x = 0;

	/* Find starts of words */
	for (c = 0; c < l; c++) {
		if (strchr(chars, p[c])) {
			word = 0;
			p[c] = 0;
			continue;
		}
		if (word == 0) {
			word = 1;
			ret[x++] = &p[c];
		}
	}

	return ret;
}


char **
strsplit(const char *line, const char *chars)
{
	return strsplit2(line, chars, NULL, 0);
}
