export TMPDIR=/home/sivertut/tmp

IMG1="man9"
IMG2="man10"
IMG_PATH="./images/input"
LINE_PATH="./lines/lines-${IMG1}-${IMG2}"
OUTPUT_PATH="./images/output/"

cmd="./morph ${IMG_PATH}/${IMG1}.jpg ${IMG_PATH}/${IMG2}.jpg ${LINE_PATH}.txt ${OUTPUT_PATH} 2"

eval "/usr/local/cuda-11.4/bin/ncu --section Occupancy ${cmd}"
