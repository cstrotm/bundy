// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <cryptolink/cryptolink.h>
#include <cryptolink/crypto_hmac.h>

#include <botan/botan.h>

namespace bundy {
namespace cryptolink {

// For Botan, we use the CryptoLink class object in RAII style
//class CryptoLinkImpl {
//private:
//    Botan::LibraryInitializer botan_init_;
//};

//CryptoLink::~CryptoLink() {
//    delete impl_;
//}

CryptoLink&
CryptoLink::getCryptoLink() {
    CryptoLink& c = getCryptoLinkInternal();
    if (c.impl_ == NULL) {
        c.initialize();
    }
    return (c);
}

CryptoLink&
CryptoLink::getCryptoLinkInternal() {
    static CryptoLink instance;
    return (instance);
}

//void
//CryptoLink::initialize() {
//    CryptoLink& c = getCryptoLinkInternal();
//   if (c.impl_ == NULL) {
//        try {
//            c.impl_ = new CryptoLinkImpl();
//        } catch (const Botan::Exception& ex) {
//            bundy_throw(InitializationError, ex.what());
//        }
//    }
//}

HMAC*
CryptoLink::createHMAC(const void* secret, size_t secret_len,
                       const HashAlgorithm hash_algorithm)
{
    return (new HMAC(secret, secret_len, hash_algorithm));
}

} // namespace cryptolink
} // namespace bundy

