#!/bin/sh
# echo $filter_path
# echo $file_format


#!/bin/sh

# for common
export common_filter_path="\
			 ! -path "*git*" \
			 ! -path ".*br16*" \
			 ! -path ".*bt16*" \
			 ! -path ".*br17*" \
			 ! -path ".*br18*" \
			 ! -path ".*br19*" \
			 ! -path ".*mini_uboot*" \
			 ! -path ".*ram1_fix_run*" \
			 ! -path ".*apps_headphone*""
			 # ! -path ".*ac461x_uboot_lib*" \

# for cscope/tag
export ct_filter_path="\
			 ! -path ".*tools*" \
			 ! -path ".*output*" \
			 ! -path ".*download*""

export ct_file_format='.*\.\(c\|h\|s\|S\|ld\|s51\)'

# for filenametags
export lookup_file_format='.*\.\(c\|h\|s\|S\|ld\|s51\|lst\|map\)'

rm tags cscope.out filenametags
# for tags
find . $common_filter_path $ct_filter_path -regex $ct_file_format | xargs ctags -R --fields=+lS --languages=+Asm --c++-kinds=+p --fields=+iaS --extra=+q

# for cscope
find . $common_filter_path $ct_filter_path -regex $ct_file_format > cscope.input

cscope -b -i cscope.input

rm cscope.input

#for filenametags
echo -e "!_TAG_FILE_SORTED\t2\t/2=foldcase/">filenametags
find . $common_filter_path -regex $lookup_file_format -type f -printf "%f\t%p\t1\n" | sort -f>>filenametags
