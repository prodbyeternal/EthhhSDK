#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

static int checks;
static void Check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}

int main()
{
    char temp[MAX_PATH]{};
    Check(GetTempPathA(MAX_PATH, temp) != 0, "temporary directory available");
    const std::string path = std::string(temp) + "ethhh_sha256_test_input.tmp";
    const std::string missing = std::string(temp) + "ethhh_sha256_missing_file.tmp";
    std::remove(missing.c_str());
    {
        std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
        f << "abc";
        Check(f.good(), "write test input");
    }
    const std::string got = eth::Sha256FileHex(path);
    std::remove(path.c_str());
    Check(got == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "known SHA-256 for abc; BCrypt property buffer and size output must not alias");
    Check(eth::Sha256FileHex(missing).empty(), "missing file returns an empty hash");
    std::printf("PASS native SHA-256 file hashing: %d assertions (BCrypt object-length/property separation)\n", checks);
    return 0;
}
