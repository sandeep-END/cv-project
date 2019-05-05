#include "Halide.h"
#include "halide_load_raw.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image_write.h"

#include "align.h"
#include "merge.h"
#include "finish.h"
#include "CImg.h"
#include <tiffio.h>
#include <cstring>
#include <algorithm>
using namespace Halide;
using namespace cimg_library;
using namespace std;

void save_tiff(Buffer<uint16_t> img){
    // TIFF *out= TIFFOpen("new.tiff", "w");
    // y -> row index
    // x -> column index
    for (int y = 0; y < 10; ++y)
    {
        for (int x = 0; x < 10; ++x)
        {
            std::cout<<img(x,y)<<" ";
        }
        std::cout<<std::endl;
    }
}


/*
 * HDRPlus Class -- Houses file I/O, defines pipeline attributes and calls
 * processes main stages of the pipeline.
 */
class HDRPlus {

    private:

        Buffer<uint16_t> imgs;

    public:

        // dimensions of pixel phone output images are 3036 x 4048

        int width;
        int height;

        const BlackPoint bp;
        const WhitePoint wp;
        const WhiteBalance wb;
        const Compression c;
        const Gain g;

        HDRPlus(Buffer<uint16_t> imgs, BlackPoint bp, WhitePoint wp, WhiteBalance wb, Compression c, Gain g) : imgs(imgs), bp(bp), wp(wp), wb(wb), c(c), g(g) {

            // assert(imgs.dimensions() == 3);         // width * height * img_idx
            // assert(imgs.width() == width);
            width = imgs.width();
            // assert(imgs.height() == height);
            height = imgs.height();
            cout<<"Setting width = "<<width<<"  and height = "<<height<<endl;
            // assert(imgs.extent(2) >= 2);            // must have at least one alternate image
        }

        Buffer<uint8_t> semi_process(){
            Func finished = semi_finish(Func(imgs),width,height,c,g);
            Buffer<uint8_t> output_img(3, width, height);

            finished.realize(output_img);
            // transpose to account for interleaved layout

            output_img.transpose(0, 1);
            output_img.transpose(1, 2);

            return output_img;
        }

        /*
         * process -- Calls all of the main stages (align, merge, finish) of the pipeline.
         */
        Buffer<uint8_t> process() {
            for (int i = 0; i < 10; ++i)
            {
                for (int j = 0; j < 10; ++j)
                {
                    std::cout<<imgs(j,i,0)<<" ";
                }
                std::cout<<std::endl;
            }

            Func alignment = align(imgs);
            Func merged = merge(imgs, alignment);
            Buffer<uint16_t> output_merged(width,height);
            merged.realize(output_merged);
            std::cout<<output_merged.width()<<"  "<<output_merged.height()<<std::endl;
            save_tiff(output_merged);
            std::cout<<"WhiteBalance"<<std::endl;
            std::cout<<wb.r<<" "<<wb.g0<<" "<<wb.g1<<" "<<wb.b<<std::endl;

            // Func inp_image = Func(imgs);
            Func finished = finish(merged, width, height, bp, wp, wb, c, g);

            ///////////////////////////////////////////////////////////////////////////
            // realize image
            ///////////////////////////////////////////////////////////////////////////

            Buffer<uint8_t> output_img(3, width, height);

            finished.realize(output_img);
            // transpose to account for interleaved layout

            output_img.transpose(0, 1);
            output_img.transpose(1, 2);

            return output_img;
        }

        /*
         * load_raws -- Loads CR2 (Canon Raw) files into a Halide Image.
         */
        // static bool load_raws(std::string dir_path, std::vector<std::string> &img_names, Buffer<uint16_t> &imgs) {

        //     int num_imgs = img_names.size();

        //     imgs = Buffer<uint16_t>(width, height, num_imgs);

        //     uint16_t *data = imgs.data();

        //     for (int n = 0; n < num_imgs; n++) {

        //         std::string img_name = img_names[n];
        //         std::string img_path = dir_path + "/" + img_name;

        //         if(!Tools::load_raw(img_path, data, width, height)) {

        //             std::cerr << "Input image failed to load" << std::endl;
        //             return false;
        //         }

        //         data += width * height;
        //     }
        //     return true;
        // }
};

/*
 * save_png -- Writes an interleaved Halide image to an output file.
 */
bool save_png(std::string dir_path, std::string img_name, Buffer<uint8_t> &img) {

    std::string img_path = dir_path + "/" + img_name;

    std::remove(img_path.c_str());

    int stride_in_bytes = img.width() * img.channels();

    if(!stbi_write_png(img_path.c_str(), img.width(), img.height(), img.channels(), img.data(), stride_in_bytes)) {

        std::cerr << "Unable to write output image '" << img_name << "'" << std::endl;
        return false;
    }

    return true;
}

void print_full(std::string file_path){
    Tools::Internal::PipeOpener f(("../tools/dcraw -c -D -6 -W -g 1 1 " + file_path).c_str(), "r");
    char buffer[1024];
    char prev_buf[1024];
    int times=0;
    // n=memcmp ( buffer1, buffer2, sizeof(buffer1) );
    while(f.f!=nullptr){
        f.readLine(buffer,1024);
        std::cout<<buffer;
        if(times==0||std::memcmp(prev_buf,buffer,1024)!=0){
            std::memcpy(prev_buf, buffer, 1024);
            times=1;
        }
        else{
            times++;
        }
        if(times==5){
            break;
        }
    }
    std::cout<<std::endl;
}

/*
 * read_white_balance -- Reads white balance multipliers from file and returns WhiteBalance.
 */
const WhiteBalance read_white_balance(std::string file_path) {

    // Tools::Internal::PipeOpener f(("../tools/dcraw -v -i " + file_path).c_str(), "r");
    
    // std::cout<<"Reading DNG"<<std::endl;
    // print_full(file_path);
    // char buf[1024];

    // while(f.f != nullptr) {

    //     f.readLine(buf, 1024);
    //     // std::cout<<buf<<std::endl;

    //     float r, g0, g1, b;

    //     if(sscanf(buf, "Camera multipliers: %f %f %f %f", &r, &g0, &b, &g1) == 4) {

    //         // float m = std::min(std::min(r, g0), std::min(g1, b));
    //         float wb[4] = {r, g0, g1, b};
    //         float m=1;
    //         std::sort(wb,wb+4);
    //         for (int i = 0; i < 4; ++i)
    //         {
    //             if(wb[i]!=0){
    //                 m=wb[i];
    //                 break;
    //             }
    //         }

    //         return {r / m, g0 / m, g1 / m, b / m};
    //     }
    // }
    // std::cout<<"Error occured"<<std::endl;
    return {1, 1, 1, 1};
}

void load_png(std::string dir_path, std::vector<std::string> &img_names, Buffer<uint16_t> &imgs){
    std::string img_path = dir_path + "/" + img_names[0];
    CImg<float> image(img_path.c_str());
    imgs = Buffer<uint16_t>(image.width(), image.height(), 3);
    for (int i = 0; i < image.width(); ++i)
    {
        for (int j = 0; j < image.height(); ++j)
        {
            for (int c = 0; c < 3; ++c)
            {
                imgs(i,j,c)=image(i,j,0,c);
            }
        }
    }
    cout<<"Successfully read the png"<<endl;
}

int main(int argc, char* argv[]) {
    
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " [-c comp -g gain (optional)] dir_path out_img raw_img1 raw_img2 [...]" << std::endl;
        return 1;
    }

    Compression c = 3.8f;
    Gain g = 1.1f;

    int i = 1;

    while(argv[i][0] == '-') {

        if(argv[i][1] == 'c') {

            c = atof(argv[++i]);
            i++;
            continue;
        }
        else if(argv[i][1] == 'g') {

            g = atof(argv[++i]);
            i++;
            continue;
        }
        else {
            std::cerr << "Invalid flag '" << argv[i][1] << "'" << std::endl;
            return 1;
        }
    }

    if (argc - i < 3) {
        std::cerr << "Usage: " << argv[0] << " [-c comp -g gain (optional)] dir_path out_img raw_img1 raw_img2 [...]" << std::endl;
        return 1;
    }

    std::string dir_path = argv[i++];
    std::string out_name = argv[i++];

    std::vector<std::string> in_names;

    while (i < argc) in_names.push_back(argv[i++]);

    Buffer<uint16_t> imgs;

    load_png(dir_path, in_names, imgs);

    // if(!HDRPlus::load_raws(dir_path, in_names, imgs)) return -1;

    const WhiteBalance wb = read_white_balance(dir_path + "/" + in_names[0]);
    const BlackPoint bp = 2050;
    const WhitePoint wp = 15464;

    HDRPlus hdr_plus = HDRPlus(imgs, bp, wp, wb, c, g);

    Buffer<uint8_t> output = hdr_plus.semi_process();

    // Buffer<uint8_t> output = hdr_plus.process();
    
    if(!save_png(dir_path, out_name, output)) return -1;

    return 0;
}
