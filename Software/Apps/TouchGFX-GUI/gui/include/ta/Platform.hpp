/**
 ******************************************************************************
 * @file    Platform.hpp
 * @brief   What the authenticator needs from the watch: files in the folder of the app, the time, the light, the form of the phone app. The program
 *          is written against this, so it also runs on the PC (for the tests).
 ******************************************************************************
 */

#ifndef TA_PLATFORM_HPP
#define TA_PLATFORM_HPP

#include <cstddef>
#include <cstdint>

namespace ta {

constexpr int kNameMax = 64;           // longest file name that is listed
constexpr int kMaxFiles = 8;

class File {
public:
    virtual ~File() {}
    virtual uint32_t size() = 0;
    virtual bool read(uint32_t offset, void *dst, uint32_t n) = 0;     // at a place
    virtual bool readNext(void *dst, uint32_t n) = 0;                  // the next bytes
    virtual bool write(const void *src, uint32_t n) = 0;               // the next bytes of a new file
};

// The one-shot form of the phone app: the values it has written into the configuration file of the app. Read once, then emptied.
struct Form {
    char code[129];                    // a long code (Base32 key) or an otpauth:// link
    char name[28];
    char issuer[24];
    int  digits;                       // 6 .. 8
    int  period;                       // seconds
    char algo[8];                      // SHA1, SHA256, SHA512
};

class Platform {
public:
    virtual ~Platform() {}
    virtual uint32_t nowMs() = 0;
    virtual uint32_t utc() = 0;                                         // seconds since 1970 (UTC), 0: the watch does not know the time
    // The files of the app folder whose name ends with ext (case does not matter), sorted by name. Returns how many.
    virtual int listFiles(const char *ext, char names[][kNameMax], int maxCount) = 0;
    // Opens a file of the app folder for reading, or creates it (replacing the old one) for writing; nullptr if that fails. The caller deletes it.
    virtual File *open(const char *name, bool forWriting) = 0;
    virtual bool exists(const char *name) = 0;
    virtual bool remove(const char *name) = 0;
    virtual bool rename(const char *from, const char *to) = 0;         // `to` is replaced if it is there
    virtual void backlight(bool alwaysOn, int percent) { (void)alwaysOn; (void)percent; }
    virtual bool readForm(Form &f) { (void)f; return false; }           // true: the form had a code
    virtual void clearForm() {}                                          // the form is emptied (the values are removed from the file)
    virtual void exitApp() = 0;
};

} // namespace ta

#endif // TA_PLATFORM_HPP
