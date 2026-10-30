// The pictures for the store: the program runs on the PC with a few made-up accounts (the same code as on the watch) and writes the screen after chosen
// steps (shots/*.ppm) and a list of frames with their time (frames/*.ppm, frames.txt) for the animated picture. Tools/make_media.py puts them together.
// args: <work folder> <output folder>
#include <ta/App.hpp>
#include "SDK/GUI/Button.hpp"
#include "disk_platform.hpp"
#include <cstdlib>
#include <fstream>

namespace B = SDK::GUI::Button;
using namespace ta;
static uint8_t fb[240 * 240];
static uint32_t now = 1000;
static std::string OUT;
static int frameNo = 0;
static FILE *list = nullptr;
static App *A = nullptr;
static DiskPlatform *P = nullptr;

static void writePpm(const std::string &path)
{
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P6\n240 240\n255\n");
    for (int i = 0; i < 240 * 240; ++i) {
        const uint8_t c = fb[i];
        const uint8_t rgb[3] = { static_cast<uint8_t>((c & 3) * 85), static_cast<uint8_t>(((c >> 2) & 3) * 85), static_cast<uint8_t>(((c >> 4) & 3) * 85) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void draw() { A->tick(now, fb); }
static void grab(const char *shot, int hold)
{
    draw();
    if (shot) writePpm(OUT + "/shots/" + shot + ".ppm");
    if (hold > 0) { char name[64]; std::snprintf(name, sizeof(name), "frames/%04d.ppm", frameNo++); writePpm(OUT + "/" + name); fprintf(list, "%s %d\n", name, hold); }
}
static void key(uint8_t k, int times = 1, int hold = 0) { for (int i = 0; i < times; ++i) { now += 120; A->onKey(k, now); draw(); if (hold > 0) grab(nullptr, hold); } }
static void wheelTo(int target, int hold)
{
    const int n = A->wheelCount();
    const int d = ((target - A->wheelCursor()) % n + n) % n;
    const int steps = d <= n / 2 ? d : n - d;
    for (int i = 0; i < steps; ++i) { key(d <= n / 2 ? B::L2_PRESS : B::L1_PRESS); if (hold > 0) grab(nullptr, hold); }
}
static void typeSecret(const char *s, int hold)
{
    for (const char *c = s; *c; ++c) { wheelTo(A->wheelIndexOfChar(*c), hold); key(B::R1_PRESS); grab(nullptr, 300); }
}
static void typePin(const char *pin, int hold)
{
    typeSecret(pin, hold);
    wheelTo(A->wheelIndexOfLabel("OK"), hold); grab(nullptr, 300); key(B::R1_PRESS);
}
// seconds pass: the time of the watch runs on
static void seconds(int n, int every, int hold, const char *shot = nullptr, int shotAt = -1)
{
    for (int i = 1; i <= n; ++i) { P->utcNow += 1; now += 1000; draw(); if (i % every == 0) grab(i == shotAt ? shot : nullptr, hold); }
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    static DiskPlatform p;
    p.root = argv[1]; p.fake = false; p.utcNow = 1893456000 + 7;   // seven seconds into a period
    P = &p;
    OUT = argv[2];
    system(("mkdir -p '" + OUT + "/shots' '" + OUT + "/frames'").c_str());
    list = fopen((OUT + "/frames.txt").c_str(), "w");
    static App app(p);
    A = &app;
    app.start();

    grab("00_choose_pin", 1500);
    typePin("4711", 60);
    grab("01_pin_again", 900);
    typePin("4711", 60);
    now += 120; draw();
    grab(nullptr, 600);
    for (int i = 0; i < 400 && app.mode() == App::Mode::Unlocking; ++i) { now += 120; draw(); }
    grab("02_no_accounts", 1800);

    // the menu: open it from the codes and go to an entry (a frame at every step)
    auto menuPick = [&](const char *name, int hold) {
        key(B::R1_PRESS); grab(nullptr, 350);
        const int idx = A->menuIndexOf(name);
        for (int i = 0; i < idx; ++i) { key(B::L2_PRESS); grab(nullptr, hold); }
    };

    // an account is pasted in the phone app: the form
    p.formHas = true;
    std::snprintf(p.form.code, sizeof(p.form.code), "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP");
    std::snprintf(p.form.name, sizeof(p.form.name), "anna@example.com"); std::snprintf(p.form.issuer, sizeof(p.form.issuer), "GitHub");
    p.form.digits = 6; p.form.period = 30; std::snprintf(p.form.algo, sizeof(p.form.algo), "SHA1");
    menuPick("ADD ACCOUNT", 160);
    key(B::R1_PRESS);
    grab("03_add_info", 3500);
    key(B::R1_PRESS); grab("04_account_added", 2200);              // look now
    key(B::R1_PRESS);
    // more accounts from a file
    std::ofstream(std::string(argv[1]) + "/import.txt") <<
        "otpauth://totp/Mail%20Co:anna?secret=KRUGKIDROVUWG2ZAMJZG653OEBTYZEBA&issuer=Mail%20Co&digits=8\n"
        "otpauth://totp/Bank:anna?secret=MFRGGZDFMZTWQ2LKNNWG23TPOBYXE43U&issuer=Bank&algorithm=SHA256&period=60\n"
        "Cloud storage, ORSXG5DJNZTWK4TMNFXHGIDUNBSXE5DB\n";
    menuPick("ADD ACCOUNT", 160);
    key(B::R1_PRESS);
    key(B::R1_PRESS); grab("05_three_added", 2200);
    key(B::R1_PRESS);
    grab("06_codes", 1500);
    seconds(6, 2, 700);
    key(B::L2_PRESS); grab("07_next_account", 1800);
    seconds(8, 2, 600, "08_seconds_running_out", 6);
    key(B::L2_PRESS); grab("09_eight_digits", 1500);
    key(B::L2_PRESS); grab("10_sixty_seconds", 1500);

    // all codes
    menuPick("ALL CODES", 160);
    key(B::R1_PRESS);
    grab("11_all_codes", 600);
    key(B::L2_PRESS, 2, 500);
    seconds(4, 2, 700);
    key(B::R2_PRESS); grab("12_menu", 1500);
    key(B::L2_PRESS, 6, 140);
    grab("13_menu_down", 1200);
    key(B::R2_PRESS);
    // remove an account
    menuPick("REMOVE", 160); key(B::R1_PRESS); grab("14_remove_which", 1400);
    key(B::L2_PRESS, 1, 300); key(B::R1_PRESS); grab("15_remove_ask", 2200);
    key(B::R2_PRESS); key(B::R2_PRESS); key(B::R2_PRESS);
    // change the PIN, lock
    menuPick("CHANGE PIN", 160); key(B::R1_PRESS);
    grab("16_change_pin", 1200);
    key(B::R2_PRESS); grab(nullptr, 200);                          // back in the menu
    key(B::R2_PRESS); grab(nullptr, 200);                          // the codes
    menuPick("LOCK", 140); key(B::R1_PRESS);
    grab("17_locked", 1500);
    typePin("4711", 60);
    for (int i = 0; i < 400 && app.mode() == App::Mode::Unlocking; ++i) { now += 120; draw(); }
    grab(nullptr, 300);
    grab("18_codes_again", 1500);
    fclose(list);
    printf("%d frames\n", frameNo);
    return 0;
}
