#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>

uint32_t pack_color(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255) {
    return r + (g << 8) + (b << 16) + (a << 24);
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

void draw_tile(std::vector<uint32_t>& img, int w, int h, int tx, int ty, int tw, int th, uint32_t color) {
    for (int i = tx; i < tx+tw; ++i) {
        for (int j = ty; j < ty+th; ++j) {
            img[i+j*w] = color;
        }
    }
}

int main() {
    const size_t win_w = 512;
    const size_t win_h = 512;
    std::vector<uint32_t> framebuffer(win_w*win_h, pack_color(60,60,60));

    const int map_w = 16;
    const int map_h = 16;
    const char map[] = "0000222222220000"\
                       "1              0"\
                       "1      11111   0"\
                       "1     0        0"\
                       "0     0  1110000"\
                       "0     3        0"\
                       "0   10000      0"\
                       "0   0   11100  0"\
                       "0   0   0      0"\
                       "0   0   1  00000"\
                       "0       1      0"\
                       "2       1      0"\
                       "0       0      0"\
                       "0 0000000      0"\
                       "0              0"\
                       "0002222222200000";
    assert(sizeof(map) == map_w*map_h+1);

    int tile_w = win_w/map_w;//the width of a tile
    int tile_h = win_h/map_h;//the height of a tile

    float player_x = 3.456; // player x position in map space
    float player_y = 2.345; // player y position in map space
    float player_a = M_PI / 2.05f; // the angle between player direction and positive x-axis
    float fov = M_PI / 3.0f;

#define map2win(X) int(X*win_w/(float)map_w)
    for (int i = 0; i < map_w; i++) {
        for (int j = 0; j < map_h; j++) {
            int tile_x = i * tile_w;
            int tile_y = j * tile_h;
            if (map[i+j*map_w] == ' ') {
                draw_tile(framebuffer, win_w, win_h, tile_x, tile_y, tile_w, tile_h, pack_color(0,220,255));
            }
            //draw player
            int px = map2win(player_x);//player x in window space
            int py = map2win(player_y);//player y in window space
            draw_tile(framebuffer, win_w, win_h, px-2, py-2, 4, 4, pack_color(255,0,0));
            //cast rays between fov
            for (int i = 0; i < 512; i++) {
                //cast 512 rays across fov centered around player_a
                float a = player_a - fov/2.0f + (i / 512.f) * fov;               
                for (float c = 0.0; c < 20.0; c+=0.05f) {
                    float cx = player_x + c * cos(a);
                    float cy = player_y + c * sin(a);
                    if (map[int(cx)+int(cy)*map_w] != ' ') break;
                    framebuffer[map2win(cx) + map2win(cy)*win_w] = pack_color(255,255,255);
                }
            }
        }
    }

    write_ppm("./out.ppm", framebuffer, win_w, win_h);
}