/*	$OpenBSD: xform.c,v 1.16 2001/08/28 12:20:43 ben Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr),
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Damien Miller (djm@mindrot.org).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * AES XTS implementation in 2008 by Damien Miller
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Copyright (C) 2008, Damien Miller
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>

#include <crypto/camellia/camellia.h>
#include <opencrypto/xform_enc.h>

static	int cml_setkey(u_int8_t **, u_int8_t *, int);
static	void cml_encrypt(caddr_t, u_int8_t *);
static	void cml_decrypt(caddr_t, u_int8_t *);
static	void cml_zerokey(u_int8_t **);

/* Encryption instances */
struct enc_xform enc_xform_camellia = {
	CRYPTO_CAMELLIA_CBC, "Camellia",
	CAMELLIA_BLOCK_LEN, CAMELLIA_BLOCK_LEN, CAMELLIA_MIN_KEY,
	CAMELLIA_MAX_KEY,
	cml_encrypt,
	cml_decrypt,
	cml_setkey,
	cml_zerokey,
	NULL,
};

/*
 * Encryption wrapper routines.
 */
static void
cml_encrypt(caddr_t key, u_int8_t *blk)
{
	camellia_encrypt((camellia_ctx *) key, (u_char *) blk, (u_char *) blk);
}

static void
cml_decrypt(caddr_t key, u_int8_t *blk)
{
	camellia_decrypt(((camellia_ctx *) key), (u_char *) blk,
	    (u_char *) blk);
}

static int
cml_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	if (len != 16 && len != 24 && len != 32)
		return (EINVAL);
	*sched = KMALLOC(sizeof(camellia_ctx), M_CRYPTO_DATA,
	    M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		camellia_set_key((camellia_ctx *) *sched, (u_char *) key,
		    len * 8);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
cml_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(camellia_ctx));
	KFREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}
