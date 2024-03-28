#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdint>
#include <cassert>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


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

void write_png(const char* filename, std::vector<uint32_t>& img, size_t width, size_t height) {
    if (!stbi_write_png(filename, width, height, 4, img.data(), width * 4)) {
        assert(false);
    }
}

void draw_tile(std::vector<uint32_t>& img, int w, int h, int tx, int ty, int tw, int th, uint32_t color) {
    for (int i = tx; i < tx+tw; ++i) {
        for (int j = ty; j < ty+th; ++j) {
            img[i+j*w] = color;
        }
    }
}

//The class represent a texture altas which contains a collection of images (texture). This class is responsible
//for loading atlas from file using stb_image library, figuring out the amount of textures the atlas has and the size of
//each texture etc. This class also provided an API that allows one to extract pixel color of specific texture in the atlas
class TextureAtlas {
    //w,h,c correspond to width, height and channel count of the input image file respectively.
    //rows, cols are user provided parameters used to specify how many rows and columns
    //the input image has, those are used to calculate the quantity and size of textures.
    //assume all texture in a texture atlas is the same size.
    int w,h,c,rows,cols;
    //texture quantity
    int tex_cnt;
    //texture size
    int tex_w, tex_h;

    //storing input texture altas image pixel data in rgba
    std::vector<uint32_t> data;

    //load image from file and initialze all data members.
    //the input image must have 4 channels (r,g,b,a). put
    //asserts to check all neccesary prerequisits.
    void load_img(const char* fname, int rows, int cols) {
        uint8_t* img_data = stbi_load(fname, &w, &h, &c, 4);
        assert(img_data != nullptr && "Failed to load image");
        assert(c == 4 && "Input image must have 4 channels (RGBA)");
        assert(rows > 0 && cols > 0 && "Rows and columns must be positive");
        assert(w % cols == 0 && h % rows == 0 && "Image dimensions must be divisible by rows and columns");

        this->rows = rows;
        this->cols = cols;
        tex_cnt = rows * cols;
        tex_w = w / cols;
        tex_h = h / rows;

        data.resize(w * h);
        for (int i = 0; i < w * h; ++i) {
            uint8_t r = img_data[i * 4];
            uint8_t g = img_data[i * 4 + 1];
            uint8_t b = img_data[i * 4 + 2];
            uint8_t a = img_data[i * 4 + 3];
            data[i] = pack_color(r,g,b,a);
        }

        stbi_image_free(img_data);
    }
public:
    TextureAtlas(const char* filename, int rows, int cols) {
        load_img(filename, rows, cols);
    }

    size_t texture_count() {
        return tex_cnt;
    }

    size_t texture_width() {
        return tex_w;
    }

    size_t texture_height() {
        return tex_h;
    }

    //return texture color by the reference parameter 'color' indexed by
    //row(r) and column(c). the floating point numbers x, y are in range [0-1]
    //which indicates the coordinates inside the texture.
    uint32_t texture_color(int r, int c, float x, float y) {
        assert(r >= 0 && r < rows && "Row index out of range");
        assert(c >= 0 && c < cols && "Column index out of range");
        assert(x >= 0 && x <= 1 && "x must be in range [0, 1]");
        assert(y >= 0 && y <= 1 && "y must be in range [0, 1]");

        int tex_x = static_cast<int>(x * tex_w);
        int tex_y = static_cast<int>(y * tex_h);
        int index = (r * tex_h + tex_y) * w + c * tex_w + tex_x;
        return data[index];
    }
};

struct Pawn {
    float x, y;
    TextureAtlas *texture;
    int tex_id;
};

void draw_sprite(std::vector<uint32_t>& img, int w, int h, std::vector<float>&depth, float dist, int tx, int ty, int tw, int th, TextureAtlas& tex, int tex_id) {
    for (int i = tx; i < tx+tw; ++i) {
        for (int j = ty; j < ty+th; ++j) {
            if (i < w/2 || i >= w || j < 0 || j > h) continue;
            if (depth[i-w/2] < dist) continue;//w/2 because the 3D view is on the right part
            depth[i-w/2] = dist;
            float sample_x = (i-tx)/(float)tw;
            float sample_y = (j-ty)/(float)th;
            uint32_t color = tex.texture_color(0, tex_id, sample_x, sample_y);
            if (color & 0xFF000000) img[i+j*w] = color;
        }
    }
}

void draw_foes(
    std::vector<uint32_t>&fb, int w, int h, 
    std::vector<float>& depth,
    std::vector<Pawn>& foes, 
    float player_x, float player_y,
    float fov,
    float player_a) {
    for (auto& foe : foes) {
        //draw foes on mini map
        auto mx = (foe.x / 16.0f) * (w/2.0f);
        auto my = (foe.y / 16.0f) * h;
        draw_tile(fb, w, h, int(mx-2), int(my-2), 4, 4, pack_color(255,255,255));

        //draw foe on 3D view
        float foe_a = atan2(foe.y - player_y, foe.x - player_x);
        //the angle between player_a which is the center of the view and the foe
        float a = foe_a - player_a;
        //if player_a map to the center of 3D view
        //then a is mapping to center + (a/fov)*win_w
        float offset = w/4 + a*(w/2)/fov;//offset from the center of the 3D view
        float sa = w/2 + offset;
        //sa is the center screen coordinate of foe, to get the top-left of foe's texture:
        float dist = sqrt(powf(foe.x-player_x, 2)+powf(foe.y-player_y, 2));
        auto sw = std::min((float)h, h/dist);
        auto sh = std::min((float)h, h/dist);;
        auto sx = sa - sw/2.0f;
        auto sy = h/2 - sh/2.0f;
        if (sx+sw < w/2 || sx > w) continue;//outside of view cone
        draw_sprite(fb, w, h, depth, dist, int(sx), int(sy), sw, sh, *foe.texture, foe.tex_id);
    }
}

int main() {
    const size_t win_w = 512*2;
    const size_t win_h = 512;
    std::vector<uint32_t> framebuffer(win_w*win_h, pack_color(60,60,60));
    std::vector<float> depth(win_w/2, 0.0f);

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

    //randomly picking n colors;
    srand(123456);
    std::vector<uint32_t> ncolors(10, 0);
    for (int i = 0; i < ncolors.size(); i++) {
        ncolors[i] = pack_color(rand()%255, rand()%255, rand()%255);
    }

    //load wall texture
    TextureAtlas wall("walltext.png", 1, 6);
    //monster texture
    TextureAtlas monster("monsters.png", 1, 4);

    int tile_w = win_w/(map_w*2);//the width of a tile
    int tile_h = win_h/map_h;//the height of a tile

    float player_x = 3.456; // player x position in map space
    float player_y = 2.345; // player y position in map space
    float player_a = M_PI / 2.05f; // the angle between player direction and positive x-axis
    float fov = M_PI / 3.0f;

     std::vector<Pawn> foes = {{5, 2, &monster, 2}, {1.834, 8.765, &monster, 0}, {2.834, 6.765, &monster, 3},{5.323, 5.365, &monster, 1}, {4.123, 10.265, &monster, 1}};
    // std::vector<Pawn> foes = {{5.323, 5.365, &monster, 1}};

#define map2win(X) int(X*win_w/((float)map_w*2.0f))

    for (int frame = 0; frame < 180; frame++) {
        std::stringstream ss;
        ss << "frame_" << std::setfill('0') << std::setw(3) << frame << ".png";

        player_a += 2*M_PI/180.0f;
        while (player_a > M_PI) player_a-=2*M_PI;
        while (player_a < -M_PI) player_a+=2*M_PI;

        for (int i = 0; i < map_w; i++) {
            for (int j = 0; j < map_h; j++) {
                int tile_x = i * tile_w;
                int tile_y = j * tile_h;
                if (map[i+j*map_w] != ' ') {
                    uint32_t c = ncolors[map[i+j*map_w]-'0'];
                    draw_tile(framebuffer, win_w, win_h, tile_x, tile_y, tile_w, tile_h, c);
                }
                //draw player
                int px = map2win(player_x);//player x in window space
                int py = map2win(player_y);//player y in window space
                draw_tile(framebuffer, win_w, win_h, px-2, py-2, 4, 4, pack_color(255,0,0));
                //cast rays between fov
                for (int i = 0; i < 512; i++) {
                    //cast 512 rays across fov centered around player_a
                    float a = player_a - fov/2.0f + (i / 512.f) * fov;
                    for (float c = 0.0; c < 20.0; c+=0.01f) {
                        float cx = player_x + c * cos(a);
                        float cy = player_y + c * sin(a);
                        //draw rays on map view (left)               
                        framebuffer[map2win(cx) + map2win(cy)*win_w] = pack_color(170,170,170);
                        //one ray generate one colum of 3D view (right)
                        if (map[int(cx)+int(cy)*map_w] != ' ') {
                            float dist = c*cos(a-player_a);
                            depth[i] = dist;
                            int l = win_h/dist;
                            // uint32_t c = ncolors[map[int(cx)+int(cy)*map_w]-'0'];
                            //check which kind of wall we are hitting
                            auto gx = cx - floor(cx);
                            auto gy = cy - floor(cy);
                            bool vertical = int(cx + 0.01*cos(M_PI-a)) != int(cx);
                            float tex_x = vertical ? gy: gx;

                            for (int j = 0; j < l; j++) {
                                uint32_t c = wall.texture_color(0, map[int(cx)+int(cy)*map_w]-'0', tex_x, j/(float)l);
                                framebuffer[win_w/2 + i + (win_h/2 - l/2 + j)*win_w] = c;
                            }
                            break;
                        }
                    }
                }

                draw_foes(framebuffer, win_w, win_h, depth, foes, player_x, player_y, fov, player_a);
            }
        }
        write_png(ss.str().c_str(), framebuffer, win_w, win_h);
        std::fill(framebuffer.begin(), framebuffer.end(),  pack_color(60,60,60));
        std::fill(depth.begin(), depth.end(),  10000.0f);
    }

}