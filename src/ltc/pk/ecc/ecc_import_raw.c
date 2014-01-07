/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 */

/* Implements ECC over Z/pZ for curve y^2 = x^3 + a*x + b
 *
 */

#include "tomcrypt.h"

#ifdef LTC_MECC

/** Import raw public or private key (public keys = ANSI X9.63 compressed or uncompressed; private keys = raw bytes)
  @param in     The input data to read
  @param inlen  The length of the input data
  @param key    [out] destination to store imported key
  @param dp     Curve parameters
  Return        CRYPT_OK on success
*/

int ecc_import_raw(const unsigned char *in, unsigned long inlen, ecc_key *key, ltc_ecc_set_type *dp)
{
   int err, size = 0, type = -1;
   void *t1, *t2, *prime, *a, *b;
   ecc_point *base;

   LTC_ARGCHK(in  != NULL);
   LTC_ARGCHK(key != NULL);
   LTC_ARGCHK(dp  != NULL);

   /* init key + temporary numbers */
   if (mp_init_multi(&key->pubkey.x, &key->pubkey.y, &key->pubkey.z, &key->k, &t1, &t2, &prime, &a, &b, NULL) != CRYPT_OK) {
      return CRYPT_MEM;
   }
   
   if (inlen == dp->size) {
     /* ######## read PRIVATE key */
     type = PK_PRIVATE;
     size = inlen;
     /* load private k */
     if ((err = mp_read_unsigned_bin(key->k, (unsigned char *)in, size)) != CRYPT_OK) {
        goto cleanup;
     }
     if (mp_iszero(key->k)) {
        err = CRYPT_INVALID_PACKET;
        goto cleanup;
     }
     /* init base point */
     if ((base = ltc_ecc_new_point()) == NULL) {
       err = CRYPT_MEM;
       goto cleanup;
     }
     /* load prime + base point */
     if ((err = mp_read_radix(prime, dp->prime, 16)) != CRYPT_OK)               { goto cleanup; }
     if ((err = mp_read_radix(base->x, dp->Gx, 16)) != CRYPT_OK)                { goto cleanup; }
     if ((err = mp_read_radix(base->y, dp->Gy, 16)) != CRYPT_OK)                { goto cleanup; }
     if ((err = mp_set(base->z, 1)) != CRYPT_OK)                                { goto cleanup; }
     /* make the public key */
     if ((err = mp_read_radix(a, dp->A, 16)) != CRYPT_OK)                       { goto cleanup; }
     if ((err = ltc_mp.ecc_ptmul(key->k, base, &key->pubkey, a, prime, 1)) != CRYPT_OK) {
       goto cleanup;
     }
     /* cleanup */
     ltc_ecc_del_point(base);
   }
   else if (in[0] == 0x04 && (inlen&1) && ((inlen-1)>>1) == dp->size) {
     /* ######## read PUBLIC key - uncompressed point */
     type = PK_PUBLIC;
     size = (inlen-1)>>1;
     /* load public.x */
     if ((err = mp_read_unsigned_bin(key->pubkey.x, (unsigned char *)in+1, size)) != CRYPT_OK) {
        goto cleanup;
     }
     /* load public.y */
     if ((err = mp_read_unsigned_bin(key->pubkey.y, (unsigned char *)in+1+size, size)) != CRYPT_OK) {
        goto cleanup;
     }
     /* set public.z */
     if ((err = mp_set(key->pubkey.z, 1)) != CRYPT_OK)                          { goto cleanup; }
   }
   else if ((in[0] == 0x02 || in[0] == 0x03) && (inlen-1) == dp->size) {
     /* ######## read PUBLIC key - compressed point */
     type = PK_PUBLIC;
     size = inlen-1;
     /* load public.x */
     if ((err = mp_read_unsigned_bin(key->pubkey.x, (unsigned char *)in+1, size)) != CRYPT_OK) {
        goto cleanup;
     }
     /* load prime + base point */
     if ((err = mp_read_radix(prime, dp->prime, 16)) != CRYPT_OK)               { goto cleanup; }
     if ((err = mp_read_radix(b, dp->B, 16)) != CRYPT_OK)                       { goto cleanup; }
     if ((err = mp_read_radix(a, dp->A, 16)) != CRYPT_OK)                       { goto cleanup; }
     /* compute x^3 */
     if ((err = mp_sqr(key->pubkey.x, t1)) != CRYPT_OK)                         { goto cleanup; }
     if ((err = mp_mulmod(t1, key->pubkey.x, prime, t1)) != CRYPT_OK)           { goto cleanup; }
     /* compute x^3 + a*x */
     if ((err = mp_mulmod(a, key->pubkey.x, prime, t2)) != CRYPT_OK)            { goto cleanup; }
     if ((err = mp_add(t1, t2, t1)) != CRYPT_OK)                                { goto cleanup; }
     /* compute x^3 + a*x + b */
     if ((err = mp_add(t1, b, t1)) != CRYPT_OK)                                 { goto cleanup; }
     /* compute sqrt(x^3 + a*x + b) */
     if ((err = mp_sqrtmod_prime(t1, prime, t2)) != CRYPT_OK)                   { goto cleanup; }
     /* adjust y */
     if ((mp_isodd(t2) && in[0] == 0x03) || (!mp_isodd(t2) && in[0] == 0x02)) {
       if ((err = mp_mod(t2, prime, key->pubkey.y)) != CRYPT_OK)                { goto cleanup; }
     }
     else {
       if ((err = mp_submod(prime, t2, prime, key->pubkey.y)) != CRYPT_OK)      { goto cleanup; }
     }
     /* set public.z */
     if ((err = mp_set(key->pubkey.z, 1)) != CRYPT_OK)                          { goto cleanup; }
   }
   else {
     err = CRYPT_INVALID_PACKET;
     goto cleanup;
   }
   
   if ((err = ltc_ecc_is_point(dp, key->pubkey.x, key->pubkey.y)) != CRYPT_OK)      {
     err = CRYPT_INVALID_PACKET;
     goto cleanup;
   }

   key->type = type;
   key->idx  = -1;
   key->dp   = dp;

   /* we're done */
   mp_clear_multi(t1, t2, prime, a, b, NULL);
   return CRYPT_OK;
cleanup:
   mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, t1, t2, prime, a, b, NULL);
   return err;
}

#endif
