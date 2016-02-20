
#include "helpers.h"
#include <png.h>

#include <nathanaeljones/imageflow/imageflow.h>
#include "fastscaling_private.h"
#include "weighting_test_helpers.h"
#include "trim_whitespace.h"
#include "string.h"
#include "lcms2.h"
#include "catch.hpp"

//TODO: https://github.com/pornel/pngquant/

TEST_CASE ("Load png", "[fastscaling]")
{

    bool success = false;

    uint8_t image_bytes_literal[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
    png_size_t image_bytes_count = sizeof(image_bytes_literal);
    png_const_voidp image_bytes = &image_bytes_literal;

    png_image image;

    /* Only the image structure version number needs to be set. */
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION;
    image.opaque = NULL;

    if (png_image_begin_read_from_memory(&image,image_bytes, image_bytes_count))
    {
        png_bytep buffer;

        /* Change this to try different formats!  If you set a colormap format
         * then you must also supply a colormap below.
         */
        image.format = PNG_FORMAT_BGRA;

        buffer =  (png_bytep)malloc (PNG_IMAGE_SIZE(image));

        if (buffer != NULL)
        {
            if (png_image_finish_read(&image, NULL/*background*/, buffer,
                                      0/*row_stride*/, NULL/*colormap for PNG_FORMAT_FLAG_COLORMAP */))
            {

                success=true;

//                if (png_image_write_to_file(&image, argv[2],
//                                            0/*convert_to_8bit*/, buffer, 0/*row_stride*/,
//                                            NULL/*colormap*/))
//                    result = 0;
//
//                else
//                    fprintf(stderr, "pngtopng: write %s: %s\n",
//                            image.message);
//
//                free(buffer);
            }

            else
            {
                fprintf(stderr, "png_image_finish_read: %s\n",
                        image.message);

                /* This is the only place where a 'free' is required; libpng does
                 * the cleanup on error and success, but in this case we couldn't
                 * complete the read because of running out of memory.
                 */
                png_image_free(&image);
            }
        }

        else
            fprintf(stderr, "pngtopng: out of memory: %lu bytes\n",
                    (unsigned long)PNG_IMAGE_SIZE(image));
    }

    else
        /* Failed to read the first argument: */
        fprintf(stderr, "png_image_begin_read_from_memory: %s\n", image.message);

    REQUIRE (success);
}



TEST_CASE ("Load png from URL", "[fastscaling]")
{

    bool success = false;

    size_t bytes_count = 0;

    uint8_t * bytes = get_bytes_cached("http://s3.amazonaws.com/resizer-images/sun_256.png", &bytes_count);
    png_size_t image_bytes_count = bytes_count;
    png_const_voidp image_bytes =bytes;

    png_image image;

    /* Only the image structure version number needs to be set. */
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION;
    image.opaque = NULL;

    if (png_image_begin_read_from_memory(&image,image_bytes, image_bytes_count))
    {
        png_bytep buffer;

        /* Change this to try different formats!  If you set a colormap format
         * then you must also supply a colormap below.
         */
        image.format = PNG_FORMAT_BGRA;

        buffer =  (png_bytep)calloc ( PNG_IMAGE_SIZE(image), sizeof(png_bytep));

        if (buffer != NULL)
        {
            if (png_image_finish_read(&image, NULL/*background*/, buffer,
                                      0/*row_stride*/, NULL/*colormap for PNG_FORMAT_FLAG_COLORMAP */))
            {

                int nonzero = (int) nonzero_count((uint8_t *) buffer, PNG_IMAGE_SIZE(image));
                if (nonzero > 0){
                    printf("nonzero buffer: %d of %d", nonzero,  PNG_IMAGE_SIZE(image));
                }
                Context context;
                Context_initialize(&context);


                BitmapBgra * source = BitmapBgra_create_header(&context, (unsigned int )(image.width), (unsigned int)(image.height));
                if (source == NULL) {
                    exit(99);
                }
                source->fmt = BitmapPixelFormat::Bgra32;
                source->stride = PNG_IMAGE_ROW_STRIDE(image);
                printf("png stride (%d), calculated (%d)\n",source->stride,  source->w * BitmapPixelFormat_bytes_per_pixel(source->fmt));
                source->alpha_meaningful = true;
                source->pixels = buffer;

                int target_width = 300;
                int target_height = 200;

                BitmapBgra * canvas = BitmapBgra_create(&context, target_width, target_height, true, Bgra32);

                RenderDetails * details = RenderDetails_create_with(&context, InterpolationFilter::Filter_Robidoux);
                details->interpolate_last_percent = 2.1f;
                details->minimum_sample_window_to_interposharpen = 1.5;
                details->havling_acceptable_pixel_loss = 0.26f;

                if (details == NULL) exit(99);
//                details->sharpen_percent_goal = 50;
//                details->post_flip_x = flipx;
//                details->post_flip_y = flipy;
//                details->post_transpose = transpose;
                //details->enable_profiling = profile;

                //Should we even have Renderer_* functions, or just 1 call that does it all?
                //If we add memory use estimation, we should keep Renderer



                if (!RenderDetails_render(&context,details, source, canvas)){

                    char error[255];
                    Context_error_message(&context, error, 255);
                    printf("%s",error);
                    exit(77);
                }
                printf("Rendered!");
                RenderDetails_destroy(&context, details);

                BitmapBgra_destroy(&context, source);
                free(buffer);

                    //TODO, write out PNG here


                struct flow_job_png_encoder_state encoder_state;

                if (!png_write_frame(&context, NULL, &encoder_state, canvas)){
                    //CONTEXT_error_return(&context);
                    exit(42);
                }else{
                    write_all_byte("outpng.png", encoder_state.buffer, encoder_state.size);
                    success= true;
                }

                BitmapBgra_destroy(&context, canvas);
                Context_terminate(&context);


            }

            else
            {
                fprintf(stderr, "png_image_finish_read: %s\n",
                        image.message);

                /* This is the only place where a 'free' is required; libpng does
                 * the cleanup on error and success, but in this case we couldn't
                 * complete the read because of running out of memory.
                 */
                png_image_free(&image);
            }
        }

        else
            fprintf(stderr, "pngtopng: out of memory: %lu bytes\n",
                    (unsigned long)PNG_IMAGE_SIZE(image));
    }

    else
        /* Failed to read the first argument: */
        fprintf(stderr, "png_image_begin_read_from_memory: %s\n", image.message);

    REQUIRE (success);
}