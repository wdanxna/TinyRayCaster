#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>

uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return r + (uint32_t(g) << 8) + (uint32_t(b) << 16) + (uint32_t(a) << 24);
}

void unpack_color(uint32_t c, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    r = uint8_t(c & 255);
    g = uint8_t((c >> 8) & 255);
    b = uint8_t((c >> 16) & 255);
    a = uint8_t((c >> 24) & 255);
}
void write_ppm(const char* filename, std::vector<uint32_t>& img, size_t width, size_t height) {
    assert(img.size() == width*height);
    std::ofstream ofs(filename, std::ios::binary);
    ofs << "P6\n" << width << " " << height << "\n255\n";
    for (int i = 0; i < width*height; i++) {
        uint8_t r,g,b,a;
        unpack_color(img[i], r, g, b, a);
        ofs << r << g << b;
    }
    ofs.close();
}



int main() {
    const size_t win_w = 512;
    const size_t win_h = 512;
    std::vector<uint32_t> framebuffer(win_w*win_h, 255);

    for (int i = 0; i < win_w; i++) {
        for (int j = 0; j < win_h; j++) {
            uint8_t r = 255 * (i/(float)win_w);
            uint8_t g = 255 * (j/(float)win_h);
            uint8_t b = 0;
            framebuffer[i+j*win_w] = pack_color(r, g, b);
        }
    }

    write_ppm("./out.ppm", framebuffer, win_w, win_h);
}