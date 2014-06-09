/*
 * Secure Remote Password 6a implementation
 * Copyright (c) 2010 Tom Cocagne. All rights reserved.
 * https://github.com/cocagne/csrp
 *
 * The MIT License (MIT)
 * 
 * Copyright (c) 2013 Tom Cocagne
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

/* 
 * 
 * Purpose:       This is a direct implementation of the Secure Remote Password
 *                Protocol version 6a as described by 
 *                http://srp.stanford.edu/design.html
 * 
 * Author:        tom.cocagne@gmail.com (Tom Cocagne)
 * 
 * Dependencies:  OpenSSL (and Advapi32.lib on Windows)
 * 
 * Usage:         Refer to test_srp.c for a demonstration
 * 
 * Notes:
 *    This library allows multiple combinations of hashing algorithms and 
 *    prime number constants. For authentication to succeed, the hash and
 *    prime number constants must match between 
 *    srp_create_salted_verification_key(), srp_user_new(),
 *    and srp_verifier_new(). A recommended approach is to determine the
 *    desired level of security for an application and globally define the
 *    hash and prime number constants to the predetermined values.
 * 
 *    As one might suspect, more bits means more security. As one might also
 *    suspect, more bits also means more processing time. The test_srp.c 
 *    program can be easily modified to profile various combinations of 
 *    hash & prime number pairings.
 */

#ifndef SRP_H
#define SRP_H


struct SRPVerifier;
struct SRPUser;

typedef enum
{
    SRP_NG_1024,
    SRP_NG_2048,
    SRP_NG_4096,
    SRP_NG_8192,
    SRP_NG_CUSTOM
} SRP_NGType;

typedef enum 
{
    SRP_SHA1, 
    SRP_SHA224, 
    SRP_SHA256,
    SRP_SHA384, 
    SRP_SHA512
} SRP_HashAlgorithm;

// [BB] Under VC++ Zandronum uses __fastcall. In order to let SRP use OpenSSL,
// which uses __cdecl, we compile srp.cpp with __cdecl and thus need to explicitly
// mark all functions in this header with this calling convention.
#ifdef _MSC_VER
#define CALL_CONV __cdecl
#else
#define CALL_CONV
#endif


/* This library will automatically seed the OpenSSL random number generator
 * using cryptographically sound random data on Windows & Linux. If this is
 * undesirable behavior or the host OS does not provide a /dev/urandom file, 
 * this function may be called to seed the random number generator with 
 * alternate data.
 *
 * The random data should include at least as many bits of entropy as the
 * largest hash function used by the application. So, for example, if a
 * 512-bit hash function is used, the random data requies at least 512
 * bits of entropy.
 * 
 * Passing a null pointer to this function will cause this library to skip
 * seeding the random number generator. This is only legitimate if it is
 * absolutely known that the OpenSSL random number generator has already
 * been sufficiently seeded within the running application.
 * 
 * Notes: 
 *    * This function is optional on Windows & Linux and mandatory on all
 *      other platforms.
 */
void CALL_CONV srp_random_seed( const unsigned char * random_data, int data_length );


/* Out: bytes_s, len_s, bytes_v, len_v
 * 
 * The caller is responsible for freeing the memory allocated for bytes_s and bytes_v
 * 
 * The n_hex and g_hex parameters should be 0 unless SRP_NG_CUSTOM is used for ng_type.
 * If provided, they must contain ASCII text of the hexidecimal notation.
 *
 */
void CALL_CONV srp_create_salted_verification_key( SRP_HashAlgorithm alg, 
                                                   SRP_NGType ng_type, const char * username,
                                                   const unsigned char * password, int len_password,
                                                   const unsigned char ** bytes_s, int * len_s, 
                                                   const unsigned char ** bytes_v, int * len_v,
                                                   const char * n_hex, const char * g_hex );
                                          

/* Out: bytes_B, len_B.
 * 
 * On failure, bytes_B will be set to NULL and len_B will be set to 0
 * 
 * The n_hex and g_hex parameters should be 0 unless SRP_NG_CUSTOM is used for ng_type
 *
 * If rfc5054_compat is non-zero the resulting verifier will be RFC 5054 compaliant. This
 * breaks compatibility with previous versions of the csrp library but is recommended
 * for new code.
 */
struct SRPVerifier * CALL_CONV srp_verifier_new( SRP_HashAlgorithm alg, SRP_NGType ng_type, const char * username,
                                                 const unsigned char * bytes_s, int len_s, 
                                                 const unsigned char * bytes_v, int len_v,
                                                 const unsigned char * bytes_A, int len_A,
                                                 const unsigned char ** bytes_B, int * len_B,
                                                 const char * n_hex, const char * g_hex,
                                                 int rfc5054_compat );


void CALL_CONV                  srp_verifier_delete( struct SRPVerifier * ver );


int CALL_CONV                   srp_verifier_is_authenticated( struct SRPVerifier * ver );


const char * CALL_CONV          srp_verifier_get_username( struct SRPVerifier * ver );

/* key_length may be null */
const unsigned char * CALL_CONV srp_verifier_get_session_key( struct SRPVerifier * ver, int * key_length );


int CALL_CONV                   srp_verifier_get_session_key_length( struct SRPVerifier * ver );


/* user_M must be exactly srp_verifier_get_session_key_length() bytes in size */
void CALL_CONV                  srp_verifier_verify_session( struct SRPVerifier * ver,
                                                             const unsigned char * user_M, 
                                                             const unsigned char ** bytes_HAMK );

/*******************************************************************************/

/* The n_hex and g_hex parameters should be 0 unless SRP_NG_CUSTOM is used for ng_type
 *
 * If rfc5054_compat is non-zero the resulting verifier will be RFC 5054 compaliant. This
 * breaks compatibility with previous versions of the csrp library but is recommended
 * for new code. 
 */
struct SRPUser * CALL_CONV      srp_user_new( SRP_HashAlgorithm alg, SRP_NGType ng_type, const char * username,
                                              const unsigned char * bytes_password, int len_password,
                                              const char * n_hex, const char * g_hex,
                                              int rfc5054_compat );
                                    
void CALL_CONV                  srp_user_delete( struct SRPUser * usr );

int CALL_CONV                   srp_user_is_authenticated( struct SRPUser * usr);


const char * CALL_CONV          srp_user_get_username( struct SRPUser * usr );

/* key_length may be null */
const unsigned char * CALL_CONV srp_user_get_session_key( struct SRPUser * usr, int * key_length );

int CALL_CONV                   srp_user_get_session_key_length( struct SRPUser * usr );

/* Output: username, bytes_A, len_A */
void CALL_CONV                  srp_user_start_authentication( struct SRPUser * usr, const char ** username, 
                                                     const unsigned char ** bytes_A, int * len_A );

/* Output: bytes_M, len_M  (len_M may be null and will always be 
 *                          srp_user_get_session_key_length() bytes in size) */
void CALL_CONV                  srp_user_process_challenge( struct SRPUser * usr, 
                                                            const unsigned char * bytes_s, int len_s, 
                                                            const unsigned char * bytes_B, int len_B,
                                                            const unsigned char ** bytes_M, int * len_M );
                                                  
/* bytes_HAMK must be exactly srp_user_get_session_key_length() bytes in size */
void CALL_CONV                  srp_user_verify_session( struct SRPUser * usr, const unsigned char * bytes_HAMK );

#endif /* Include Guard */
