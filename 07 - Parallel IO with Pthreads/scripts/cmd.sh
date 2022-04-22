IMG1="man9"
IMG2="man10"
ROOTDIR="."
IMG_PATH="${ROOTDIR}/input/images"
OUTPUT_PATH="${ROOTDIR}/output/images/"
LINES="${ROOTDIR}/input/lines/lines-${IMG1}-${IMG2}"
STEPS=$1

cmd="${ROOTDIR}/morph ${IMG_PATH}/${IMG1}.jpg ${IMG_PATH}/${IMG2}.jpg ${LINES}.txt ${OUTPUT_PATH} ${STEPS}"
echo $cmd
eval "$cmd"