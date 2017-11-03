/*
 * Copyright 2017 Patrick O. Perry.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rutf8.h"


static const char *encoding_name(cetype_t ce)
{
	switch (ce) {
	case CE_LATIN1:
		return "latin1";
	case CE_UTF8:
		return "UTF-8";
	case CE_SYMBOL:
		return "symbol";
	case CE_BYTES:
		return "bytes";
	case CE_ANY:
	case CE_NATIVE:
	default:
		return "unknown";
	}
}


SEXP rutf8_as_utf8(SEXP sx)
{
	SEXP ans, sstr;
	struct utf8lite_message msg;
	struct utf8lite_text text;
	const uint8_t *str;
	cetype_t ce;
	size_t size;
	R_xlen_t i, n;
	int raw, duped;;

	if (sx == R_NilValue) {
		return R_NilValue;
	}
	if (!isString(sx)) {
		error("argument is not a character object");
	}

	ans = sx;
	duped = 0;

	n = XLENGTH(sx);
	for (i = 0; i < n; i++) {
		CHECK_INTERRUPT(i);

		sstr = STRING_ELT(sx, i);
		if (sstr == NA_STRING) {
			continue;
		}

		ce = getCharCE(sstr);
		raw = rutf8_encodes_utf8(ce) || ce == CE_BYTES;

		if (raw) {
			str = (const uint8_t *)CHAR(sstr);
			size = (size_t)XLENGTH(sstr);
		} else {
			str = (const uint8_t *)rutf8_translate_utf8(sstr);
			size = strlen((const char *)str);
		}

		if (utf8lite_text_assign(&text, str, size, 0, &msg)) {
			if (ce == CE_BYTES) {
				Rf_error("argument entry %"PRIu64
					 " cannot be cast"
					 " from \"bytes\" to \"UTF-8\": %s",
					 (uint64_t)i + 1, msg.string);
			} else if (raw) {
				Rf_error("argument entry %"PRIu64
					 " is incorrectly marked as \"UTF-8\""
					 ": %s",
					 (uint64_t)i + 1,
					 msg.string);
			} else {
				Rf_error("argument entry %"PRIu64
					 " cannot be converted"
					 " from \"%s\" encoding to \"UTF-8\";"
					 " calling 'enc2utf8'"
					 " returns malformed data: %s",
					 (uint64_t)i + 1,
					 encoding_name(ce),
					 msg.string);
			}
		}

		if (!raw || ce == CE_BYTES || ce == CE_NATIVE) {
			if (!duped) {
				PROTECT(ans = duplicate(ans));
				duped = 1;
			}
			SET_STRING_ELT(ans, i,
				       mkCharLenCE((const char *)str,
					           size, CE_UTF8));
		}
	}

	if (duped) {
		UNPROTECT(1);
	}

	return ans;
}
