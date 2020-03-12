#!/bin/bash

parse_dir() {
    # Parse a directory from a url into an array of items
    url=$1
    echo `curl -sSL -w -L '%{http_code} %{url_effective}' $url | grep href | sed 's/.*href="//' | sed 's/".*//' | grep '^[a-zA-Z].*'`
}

OUTPUT="releases.json"

# Clear and recreate target file
rm $OUTPUT
touch $OUTPUT

json_string="["
add_comma=false

GOOD_ARCHES="x86_64 aarch64 ppc64le i586"

# Extract image urls from the mirrors directory
for variant in "workstation" "server" "education" "simply"
do
    # echo "variant = $variant"
    
    variant_url="https://mirror.yandex.ru/altlinux/p9/images/$variant/"

    # echo $variant_url
    
    arches=$(parse_dir $variant_url)
    # NOTE: Remove "/" from the end of arches, need them to be clean for later processing
    arches="${arches///}"
    # echo "arches for $variant = $arches"

    for arch in $arches
    do
        # Skip bad arches
        if [[ $GOOD_ARCHES != *$arch* ]]
        then
           continue
        fi

        # echo "arch = $arch"

        arch_url="$variant_url$arch"

        images=$(parse_dir $arch_url)

        for image in $images
        do
            # Skip non-image links
            case $image in
              *"SUM"*) continue ;;
              *".txt"*) continue ;;
              *".list"*) continue ;;
            # Skip "live" images
              *"live"*) continue ;;
            # Skip "beta" images
              *"beta"*) continue ;;
            esac

            # Convert image to fields for json
            link="$arch_url/$image"
            # echo "link = $link"
            # echo "variant = $variant"
            # echo "arch = $arch"

            board="UNKNOWN"
            case $arch in
                "x86_64") board="pc";;
                "i586") board="ibmpc";;
                "ppc64le") board="powerpc";;
                "aarch64")
                    case $image in
                        *"tegra"*) board="jetson-nano";;
                        *"rpi4"*) board="rpi4";;
                        *"tar.xz"*) board="rpi3";;
                        *"iso"*) board="arm";;
                        *) board="UNKNOWN AARCH64";;
                    esac
                    ;;
                *) board="UNKNOWN"
            esac

            # Extract image type after "$arch." portion
            imageType=$(echo $image | sed "s/.*$arch.//g")
            # echo "imageType = $imageType"

            if [ $add_comma == true ]
            then
                json_string="${json_string},"
            else
                add_comma=true
            fi

            # NOTE: version hardcoded, can't extract it from url directly because it's position varies and sometimes it's not there at all
            json_string="${json_string}\n\t{\n\t\t\"link\":\"$link\",\n\t\t\"variant\":\"$variant\",\n\t\t\"version\":\"9.0\",\n\t\t\"arch\":\"$arch\",\n\t\t\"board\":\"$board\",\n\t\t\"imageType\":\"$imageType\",\n\t\t\"md5\":\"\"\n\t}"
        done
    done
done

json_string="${json_string}\n]"

printf "$json_string" >> $OUTPUT

