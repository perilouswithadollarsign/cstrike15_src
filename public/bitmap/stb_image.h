/* stbi-1.29 - public domain JPEG/PNG reader - http://nothings.org/stb_image.c
   when you control the images you're loading
                                     no warranty implied; use at your own risk

   QUICK NOTES:
      Primarily of interest to game developers and other people who can
          avoid problematic images and only need the trivial interface

      JPEG baseline (no JPEG progressive)
      PNG 8-bit only

      TGA (not sure what subset, if a subset)
      BMP non-1bpp, non-RLE
      PSD (composited view only, no extra channels)

      GIF (*comp always reports as 4-channel)
      HDR (radiance rgbE format)
      PIC (Softimage PIC)

      - decoded from memory or through stdio FILE (define STBI_NO_STDIO to remove code)
      - supports installable dequantizing-IDCT, YCbCr-to-RGB conversion (define STBI_SIMD)

   Latest revisions:
      1.29 (2010-08-16) various warning fixes from Aurelien Pocheville 
      1.28 (2010-08-01) fix bug in GIF palette transparency (SpartanJ)
      1.27 (2010-08-01) cast-to-uint8 to fix warnings (Laurent Gomila)
                        allow trailing 0s at end of image data (Laurent Gomila)
      1.26 (2010-07-24) fix bug in file buffering for PNG reported by SpartanJ
      1.25 (2010-07-17) refix trans_data warning (Won Chun)
      1.24 (2010-07-12) perf improvements reading from files
                        minor perf improvements for jpeg
                        deprecated type-specific functions in hope of feedback
                        attempt to fix trans_data warning (Won Chun)
      1.23              fixed bug in iPhone support
      1.22 (2010-07-10) removed image *writing* support to stb_image_write.h
                        stbi_info support from Jetro Lauha
                        GIF support from Jean-Marc Lienher
                        iPhone PNG-extensions from James Brown
                        warning-fixes from Nicolas Schulz and Janez Zemva
      1.21              fix use of 'uint8' in header (reported by jon blow)
      1.20              added support for Softimage PIC, by Tom Seddon

   See end of file for full revision history.

   TODO:
      stbi_info support for BMP,PSD,HDR,PIC
      rewrite stbi_info and load_file variations to share file handling code
           (current system allows individual functions to be called directly,
           since each does all the work, but I doubt anyone uses this in practice)


 ============================    Contributors    =========================
              
 Image formats                                Optimizations & bugfixes
    Sean Barrett (jpeg, png, bmp)                Fabian "ryg" Giesen
    Nicolas Schulz (hdr, psd)                                                 
    Jonathan Dummer (tga)                     Bug fixes & warning fixes           
    Jean-Marc Lienher (gif)                      Marc LeBlanc               
    Tom Seddon (pic)                             Christpher Lloyd           
    Thatcher Ulrich (psd)                        Dave Moore                 
                                                 Won Chun                   
                                                 the Horde3D community      
 Extensions, features                            Janez Zemva                
    Jetro Lauha (stbi_info)                      Jonathan Blow              
    James "moose2000" Brown (iPhone PNG)         Laurent Gomila                             
                                                 Aruelien Pocheville

 If your name should be here but isn't, let Sean know.

*/

#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

// To get a header file for this, either cut and paste the header,
// or create stb_image.h, #define STBI_HEADER_FILE_ONLY, and
// then include stb_image.c from it.

#define STBI_HEADER_FILE_ONLY
#include "stb_image.c"
