// A Platform on the disk of the PC for the tests of the authenticator: the "app folder" is a folder.
#ifndef TA_DISK_PLATFORM_HPP
#define TA_DISK_PLATFORM_HPP
#include <ta/Platform.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

struct DiskFile : ta::File {
    FILE *f = nullptr; uint32_t sz = 0; bool writing; uint32_t wpos = 0;
    DiskFile(const std::string &p, bool w) : writing(w) { f = fopen(p.c_str(), w ? "w+b" : "rb"); if (f && !w) { fseek(f, 0, SEEK_END); sz = static_cast<uint32_t>(ftell(f)); fseek(f, 0, SEEK_SET); } }
    ~DiskFile() override { if (f) fclose(f); }
    uint32_t size() override { return writing ? wpos : sz; }
    bool read(uint32_t off, void *dst, uint32_t n) override { return f && !writing && fseek(f, off, SEEK_SET) == 0 && fread(dst, 1, n, f) == n; }
    bool readNext(void *dst, uint32_t n) override { return f && !writing && fread(dst, 1, n, f) == n; }
    bool write(const void *src, uint32_t n) override { if (!f || !writing || fwrite(src, 1, n, f) != n) return false; wpos += n; return true; }
};

struct DiskPlatform : ta::Platform {
    std::string root;
    uint32_t fakeNow = 1000; bool fake = false;
    uint32_t utcNow = 1700000000;                 // what utc() says (0: not known)
    int backlightOn = 0, backlightOff = 0;
    bool exited = false;
    // the form of the phone app
    bool formHas = false; ta::Form form{}; int formCleared = 0;
    uint32_t nowMs() override { if (fake) return fakeNow += 3; return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
    uint32_t utc() override { return utcNow; }
    static bool ends(const std::string &s, const std::string &e) { if (s.size() < e.size()) return false; for (size_t i = 0; i < e.size(); ++i) if (tolower(s[s.size() - e.size() + i]) != e[i]) return false; return true; }
    int listFiles(const char *ext, char names[][ta::kNameMax], int maxCount) override {
        std::vector<std::string> v; DIR *dp = opendir(root.c_str()); if (!dp) return 0;
        while (dirent *e = readdir(dp)) { std::string n = e->d_name; if (n[0] == '.') continue; struct stat st; stat((root + "/" + n).c_str(), &st); if (!S_ISDIR(st.st_mode) && ends(n, ext)) v.push_back(n); }
        closedir(dp); std::sort(v.begin(), v.end());
        int k = 0; for (auto &s : v) if (k < maxCount) snprintf(names[k++], ta::kNameMax, "%s", s.c_str()); return k;
    }
    ta::File *open(const char *name, bool w) override { if (failTmp && w && !std::strcmp(name, "vault.tmp")) return nullptr; auto *f = new DiskFile(root + "/" + name, w); if (!f->f) { delete f; return nullptr; } return f; }
    bool exists(const char *name) override { return access((root + "/" + name).c_str(), F_OK) == 0; }
    // what a file held at the moment it was removed (for the test that the keys are written over)
    std::vector<std::pair<std::string, std::string>> removed;
    bool failTmp = false;                                  // writing vault.tmp fails
    bool remove(const char *name) override {
        if (FILE *f = fopen((root + "/" + name).c_str(), "rb")) { std::string c; char b[512]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) c.append(b, n); fclose(f); removed.push_back({ name, c }); }
        return ::remove((root + "/" + name).c_str()) == 0; }
    bool rename(const char *a, const char *b) override { return ::rename((root + "/" + a).c_str(), (root + "/" + b).c_str()) == 0; }
    void backlight(bool on, int) override { if (on) ++backlightOn; else ++backlightOff; }
    bool readForm(ta::Form &f) override { if (!formHas) return false; f = form; return true; }
    void clearForm() override { formHas = false; ++formCleared; }
    void exitApp() override { exited = true; }
};
#endif
