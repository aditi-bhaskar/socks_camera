// // this program takes 5 photos with increasing exposure/gain, then displays the five pictures in order 3 times

// // for now, don't compile so main() function doesn't conflict with the one in main.c
// // if you wanna try running this, remove this ifdef and put it in main.c
// // #if 0


// // use this to dump takeaways into the file
// // ./run.sh > imgs.txt 2>&1



// #include "lib.h"
// #include "camera.h"
// #include "display.h"
// #include "mmu.h"
// #include "sys_timer.h"

// #define FB_WIDTH  640
// #define FB_HEIGHT 480
// // #define FB_WIDTH  1920
// // #define FB_HEIGHT 1080


// #define N 2 
// int expos[] = {20000, 40000};
// float gain[] = {8.0, 8.0};


// static CameraBuffer bufs[N];

// // Bilinear debayer for 16-bit RGGB Bayer pattern
// static void debayer(uint16_t* bayer, uint32_t* rgb, uint32_t w, uint32_t h, 
//                     uint32_t in_stride_bytes, uint32_t out_stride_pixels) {
//     uint32_t stride = in_stride_bytes / 2;
//     // printk("stride: %d\nout stride: %d\n", stride, out_stride_pixels);
    
//     uint32_t ravg = 0;
//     uint32_t gavg = 0;
//     uint32_t bavg = 0;
    
//     uint32_t skip = 1;
//     for (uint32_t y = 1; y < h - 1; y += skip) {
//         for (uint32_t x = 1; x < w - 1; x += skip) {
//             uint16_t* p = bayer + y * stride + x;

//             uint32_t r, g, b;
//             if ((y & 1) == 0) {
//                 if ((x & 1) == 0) {
//                     r = p[0];
//                     g = (p[-1] + p[1] + p[-(int)stride] + p[stride]) >> 2;
//                     b = (p[-(int)stride-1] + p[-(int)stride+1] + p[stride-1] + p[stride+1]) >> 2;
//                 } else {
//                     r = (p[-1] + p[1]) >> 1;
//                     g = p[0];
//                     b = (p[-(int)stride] + p[stride]) >> 1;
//                 }
//             } else {
//                 if ((x & 1) == 0) {
//                     r = (p[-(int)stride] + p[stride]) >> 1;
//                     g = p[0];
//                     b = (p[-1] + p[1]) >> 1;
//                 } else {
//                     r = (p[-(int)stride-1] + p[-(int)stride+1] + p[stride-1] + p[stride+1]) >> 2;
//                     g = (p[-1] + p[1] + p[-(int)stride] + p[stride]) >> 2;
//                     b = p[0];
//                 }
//             }

//             r = r >> 2;
//             g = g >> 2;
//             b = b >> 2;
//             if (r > 255) r = 255;
//             if (g > 255) g = 255;
//             if (b > 255) b = 255;

//             rgb[y * out_stride_pixels/skip + x/skip] = (r << 16) | (g << 8) | b;

//             // uncomment this if you want the to see the RAW bayers instead of RGB
//             /*
//             {
//                 uint32_t v = (p[0] >> 2);
//                 rgb[y * out_stride_pixels/skip + x/skip] = (v << 16) | (v << 8) | v;
//             }
//             */
//         }
//     }
// }



// void main() {
//     mmu_enable_caches();

//     printk("\n\n\nin shot_example.c\n\n\n");
//     printk("jankencamera starting...\n");

//     if (!display_init(FB_WIDTH, FB_HEIGHT)) {
//         printk("display init failed\n");
//         return;
//     }

//     if (!camera_init()) {
//         printk("camera init failed\n");
//         return;
//     }

//     if (!camera_set_format(FB_WIDTH, FB_HEIGHT, CAM_FMT_BAYER_10)) {
//         printk("camera set format failed\n");
//         return;
//     }

//     // int expos[] = { 2500, 5000, 10000, 20000, 40000 };
//     // float gain[] = { 2.0, 4.0, 8.0, 8.0, 8.0 };

//     if (!camera_start()) {
//         printk("camera start failed\n");
//         return;
//     }

//     CameraConfig cfg = camera_get_config();
//     DisplayConfig disp = display_get_config();
//     printk("streaming %dx%d\n", cfg.width, cfg.height);

//     const int SEC = 1000000;
//     int ts[] = { 0, SEC * 1, SEC * 2, SEC * 3, SEC * 4 };
//     CameraFrame frames[N];
//     CameraShot shots[N];
//     for (int i = 0; i < N; i++) {
//         frames[i].buf = &bufs[i];
//         shots[i].ts = ts[i];
//         shots[i].gain = gain[i];
//         shots[i].exposure = expos[i];
//     }

//     mmu_flush_dcache();

//     uint32_t cap = camera_capture_frames(frames, shots, N);
//     printk("cap: %d\n", cap);
//     if (cap != N) {
//         rpi_reset();
//     }

//     for (int i = 0; i < N * 3; i++) {
//         int j = i % N;

//         printk("displaying buf %x\n", frames[j].buf);
//         debayer((uint16_t*) frames[j].buf, display_get_buffer(),
//                 cfg.width, cfg.height, cfg.stride, disp.pitch / 4);
//         display_swap();

//         CameraFrame frame = frames[j];
//         printk("expos: %d\ngain: %f\nts: %d\ndelay: %d\nerror: %d\n",
//                 frame.shot.exposure, frame.shot.gain, frame.shot.ts, frame.shot.delay, frame.shot.error);
//         sys_timer_delay_ms(1000);
//     }

//     camera_stop();

//     printk("\n\nGOING TO DUMP CAMERA CAPTURE\n\n");

//     // Uncomment this if you wanna print out the RAW bayer values (pipe output to file or something)
//     // This prints 640x480 10-bit values per line in row major order, for 5 lines (one for each image)
//     // /*
//     for (int i = 0; i < N; i++) {
//         uint16_t* bayer = (uint16_t*) frames[i].buf;
//         for (uint32_t y = 0; y < cfg.height; y+=13) {
//             for (uint32_t x = 0; x < cfg.width; x+=13) {
//                 uint16_t* p = bayer + y * cfg.stride / 2 + x;
//                 printk("%d ", p[0]);
//             }
//         }
//         printk("\n");
//     }
//     // */
// }

// // #endif
