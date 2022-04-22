IMG1="man9"
IMG2="man10"
ROOTDIR="."
IMG_PATH="./images/input"
LINE_PATH="./lines/lines-${IMG1}-${IMG2}"
OUTPUT_PATH="./images/output/"
STEPS=10

rm ${OUTPUT_PATH}*.png
cmd="${ROOTDIR}/morph ${IMG_PATH}/${IMG1}.jpg ${IMG_PATH}/${IMG2}.jpg ${LINE_PATH}.txt ${OUTPUT_PATH} ${STEPS}"
echo $cmd
eval "$cmd"