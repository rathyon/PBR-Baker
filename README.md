# PBR Baker
 Simple OpenGL based program that precomputes the irradiance and specular maps needed for PBR shading.

 It loads HDR format images, converting them from equirectangular to cubemap and saving them as individual DDS files (with mipmaps for the specular map).

 Dependencies include OpenGL 3.3, OpenGL Image (GLI), OpenGL Mathematics (GLM) and stb_image.h, all included with the project as is.
 Built with Visual Studio 2019, simply place all input images in the input directory and the results will be saved in the appropriate folder in the output directory.
 To change the baking parameters, change the definitions at the start of main.cpp to your liking.
 Sample image courtesy of HDRI Haven (https://hdrihaven.com/).