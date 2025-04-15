#include <M5Cardputer.h>

static constexpr const size_t record_number     = 256;
static constexpr const size_t record_length     = 240;
static constexpr const size_t record_size       = record_number * record_length;
static constexpr const size_t record_samplerate = 17000;

void drawMicWaveform() {
    appsMenu = true;
    M5Cardputer.Display.clear();
    static int16_t prev_y[record_length];
    static int16_t prev_h[record_length];
    static size_t rec_record_idx  = 2;
    static size_t draw_record_idx = 0;
    static int16_t *rec_data      = nullptr;

    if (!rec_data) {
        rec_data = (int16_t*)heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);
        memset(rec_data, 0, record_size * sizeof(int16_t));
    }

    static constexpr int shift = 6;
    auto data = &rec_data[rec_record_idx * record_length];

    if (M5Cardputer.Mic.record(data, record_length, record_samplerate)) {
        data = &rec_data[draw_record_idx * record_length];
        int32_t w = M5Cardputer.Display.width();
        if (w > record_length - 1) {
            w = record_length - 1;
        }
        for (int32_t x = 0; x < w; ++x) {
            M5Cardputer.Display.writeFastVLine(x, prev_y[x], prev_h[x], TFT_BLACK);
            int32_t y1 = (data[x] >> shift);
            int32_t y2 = (data[x + 1] >> shift);
            if (y1 > y2) {
                int32_t tmp = y1;
                y1 = y2;
                y2 = tmp;
            }
            int32_t y = ((M5Cardputer.Display.height()) >> 1) + y1;
            int32_t h = ((M5Cardputer.Display.height()) >> 1) + y2 + 1 - y;
            prev_y[x] = y;
            prev_h[x] = h;
            M5Cardputer.Display.writeFastVLine(x, y, h, WHITE);
        }

        M5Cardputer.Display.display();

        if (++draw_record_idx >= record_number) {
            draw_record_idx = 0;
        }
        if (++rec_record_idx >= record_number) {
            rec_record_idx = 0;
        }
    }
}
