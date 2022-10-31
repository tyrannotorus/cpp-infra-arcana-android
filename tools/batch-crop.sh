#!/usr/bin/env sh

# NOTE: Configure by editing these parameters:
# --------------------------------------------------
origin_size="24x24"
target_size="20x20"
crop_arg="20x20+2+2"

tiles_dir="installed_files/gfx/tiles"
# --------------------------------------------------

if [ ! -d ${tiles_dir} ]; then
        echo "Not a directory: ${tiles_dir}"
        exit 1
fi

for origin_file in $(find ${tiles_dir}/${origin_size}/ -name '*.png'); do
        filename=$(basename ${origin_file})

        target_file=${tiles_dir}/${target_size}/${filename}

        # The tool "convert" is used for cropping the images. From the convert
        # man page:

        # NAME
        # convert - convert between image formats as well as resize an image,
        # blur, crop, despeckle, dither, draw on, flip, join, re-sample, and
        # much more.
        #
        # OVERVIEW
        # The convert-im6.q16 program is a member of the ImageMagick-ims6.q16(1)
        # suite of tools.  Use it to convert between image formats as well as
        # resize an image, blur, crop, despeckle, dither, draw on, flip, join,
        # re-sample, and much more.

        convert \
                ${origin_file} \
                -crop ${crop_arg} \
                -colorspace sRGB \
                -type truecolor \
                -define colorspace:auto-grayscale=off \
                -define png:color-type=6 \
                ${target_file}
done
