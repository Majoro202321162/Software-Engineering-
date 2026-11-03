#include "infra/Infrastructure.h"
#include "common/Exceptions.h"

#include <sodium.h>

namespace lms {

LibsodiumArgon2idHasher::LibsodiumArgon2idHasher() {
    if (sodium_init() < 0) throw LmsException("The password library could not start.");
}

// crypto_pwhash_str produces an Argon2id string with its own random 16-byte salt and
// parameters. Interactive cost limits keep login near 1 second on old machines (CF7).
// The limits are set here only, so tuning needs no other change (risk R12).
std::string LibsodiumArgon2idHasher::hash(const std::string& password) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
        throw LmsException("The password could not be processed (out of memory).");
    return out;
}

bool LibsodiumArgon2idHasher::verify(const std::string& password, const std::string& hash) {
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

std::string LibsodiumArgon2idHasher::generateTemporaryPassword() {
    static const char letters[] = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ";
    static const char digits[] = "23456789";
    std::string pw;
    for (int i = 0; i < 8; ++i) pw += letters[randombytes_uniform(sizeof letters - 1)];
    pw += digits[randombytes_uniform(sizeof digits - 1)];
    pw += digits[randombytes_uniform(sizeof digits - 1)];
    return pw;
}

} // namespace lms
