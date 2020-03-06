#!/bin/bash

# TODO: where is rescue and simply data on getalt?

TEMP_GETALT_DIR="TEMP-getalt"

echo "Downloading getalt.org sources"
git clone http://git.altlinux.org/people/sin/private/getalt.git $TEMP_GETALT_DIR

echo "Generating metadata"

# Generate empty json's for writing
for lang in "en" "ru"
do
    jq -n '[ { "subvariant": "workstation", "name": "", "summary": "", "icon": "qrc:/logos/workstation", "screenshots": [], "description": "" }, { "subvariant": "server", "name": "", "summary": "", "icon": "qrc:/logos/server", "screenshots": [], "description": "" }, { "subvariant": "education", "name": "", "summary": "", "icon": "qrc:/logos/education", "screenshots": [], "description": "" } ]' > metadata_$lang.json
done

function extract_all_members {
    local yml_file=$1
    local yml_index=$2
    local json_index=$3

    for lang in "en" "ru"
    do
        function extract_member {
            # Extract member named yml_name in yml_file at yml_index
            # into a member named json_name into metadata_$lang.json
            # at json_index 
            local yml_name=$1
            local json_name=$2

            local json_file=metadata_$lang.json

            # Read member from yml file
            member=$(yq .members[$yml_index].$yml_name $yml_file)

            # Remove "&nbsp;" from member
            member=${member//&nbsp;/}
            # Replace "&colon;" in member with ":"
            member=$(echo "$member" | sed -e 's/&colon;/:/g')

            # For summaries, replace newlines with spaces
            if [ $json_name == "summary" ]; then
                member=$(echo "$member" | sed -e 's/\\n/ /g')
            fi

            # Write member to json file
            jq ".[$json_index].$json_name = $member" $json_file > tmp.$$.json && mv -f tmp.$$.json $json_file
        }

        extract_member "name_$lang" "name"
        extract_member "descr_$lang" "summary"
        extract_member "descr_full_$lang" "description"
    done 
}

# workstation
extract_all_members "$TEMP_GETALT_DIR/_data/sections/1-basic.yml" 0 0
# server
extract_all_members "$TEMP_GETALT_DIR/_data/sections/1-basic.yml" 1 1
# education
extract_all_members "$TEMP_GETALT_DIR/_data/sections/2-additional.yml" 0 2

rm -rf $TEMP_GETALT_DIR
