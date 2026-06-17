#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#endif

static int write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, size, f) != size) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int file_size(const char *path) {
    struct stat sb;
    if (stat(path, &sb) != 0) return -1;
    return (int)sb.st_size;
}

int main(void) {
    const char *tmpdir = "tests/tmp";
#ifdef _WIN32
    _mkdir(tmpdir);
#else
    mkdir(tmpdir, 0777);
#endif

    const unsigned char jpeg[] = {
        0xFF,0xD8,0xFF,0xE1,0x00,0x10,'E','x','i','f',0,0,'M','M',0,0,0,0,0,0,
        0xFF,0xDA,0x00,0x0C,0x03,0x01,0x00,0x02,0x11,0x03,0x11,0x00,0x3F,0x00,
        0xFF,0xD9
    };
    const unsigned char png[] = {
        137,80,78,71,13,10,26,10,
        0,0,0,13,'I','H','D','R',0,0,0,1,0,0,0,1,8,2,0,0,0,0x90,0x77,0x53,0xDE,
        0,0,0,10,'I','D','A','T',8,29,1,0,0,255,255,0,0,0,1,0,1,0,0x37,0xBA,0xC7,0x3B,
        0,0,0,0,'I','E','N','D',0xAE,0x42,0x60,0x82
    };
    const unsigned char pdf[] = "%PDF-1.4\n1 0 obj<< /Type /Catalog /Pages 2 0 R /Metadata 3 0 R >>\nendobj\n3 0 obj<</Type /Metadata /Subtype /XML>>\nendobj\n%%EOF\n";
    const unsigned char mp3[] = {
        'I','D','3',4,0,0,0,0,0,5,
        'a','b','c','d','e'
    };
    const unsigned char wav[] = {
        'R','I','F','F',0x24,0x00,0x00,0x00,'W','A','V','E',
        'f','m','t',' ',0x10,0x00,0x00,0x00,1,0,1,0,0x44,0xAC,0,0,0x88,0x58,1,0,2,0,16,0,
        'd','a','t','a',0,0,0,0
    };

    printf("[INFO] Writing test files...\n");
    write_file("tests/dummy.jpg", jpeg, sizeof(jpeg));
    write_file("tests/dummy.png", png, sizeof(png));
    write_file("tests/dummy.pdf", pdf, sizeof(pdf) - 1);
    write_file("tests/dummy.mp3", mp3, sizeof(mp3));
    write_file("tests/dummy.wav", wav, sizeof(wav));

    int before_jpeg = file_size("tests/dummy.jpg");
    int before_png = file_size("tests/dummy.png");
    int before_pdf = file_size("tests/dummy.pdf");
    int before_mp3 = file_size("tests/dummy.mp3");
    int before_wav = file_size("tests/dummy.wav");

    printf("[INFO] Before scrubbing:\n");
    printf("  JPEG: %d bytes\n", before_jpeg);
    printf("  PNG:  %d bytes\n", before_png);
    printf("  PDF:  %d bytes\n", before_pdf);
    printf("  MP3:  %d bytes\n", before_mp3);
    printf("  WAV:  %d bytes\n", before_wav);

    printf("[INFO] Running climetadata scrubber...\n");
#ifdef _WIN32
    int rc = system("climetadata tests");
#else
    int rc = system("./climetadata tests");
#endif
    if (rc != 0) {
        fprintf(stderr, "[FAIL] climetadata execution failed with exit code %d\n", rc);
        return EXIT_FAILURE;
    }

    int after_jpeg = file_size("tests/dummy.jpg");
    int after_png = file_size("tests/dummy.png");
    int after_pdf = file_size("tests/dummy.pdf");
    int after_mp3 = file_size("tests/dummy.mp3");
    int after_wav = file_size("tests/dummy.wav");

    printf("[INFO] After scrubbing:\n");
    printf("  JPEG: %d bytes\n", after_jpeg);
    printf("  PNG:  %d bytes\n", after_png);
    printf("  PDF:  %d bytes\n", after_pdf);
    printf("  MP3:  %d bytes\n", after_mp3);
    printf("  WAV:  %d bytes\n", after_wav);

    if (after_jpeg <= 0 || after_png <= 0 || after_pdf <= 0 || after_mp3 <= 0 || after_wav <= 0) {
        fprintf(stderr, "[FAIL] file corruption detected\n");
        return EXIT_FAILURE;
    }

    printf("\n[SUCCESS] All files remain intact and non-zero after metadata scrubbing\n");
    return EXIT_SUCCESS;
}

