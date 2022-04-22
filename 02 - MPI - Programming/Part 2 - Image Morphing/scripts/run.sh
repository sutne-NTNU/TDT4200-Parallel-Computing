IMG_PATH="./images"
LINE_PATH="./lines"
OUTPUT_PATH="./out/images"

IMG1="${IMG_PATH}/woman-1.jpg"
IMG2="${IMG_PATH}/woman-2.jpg"
LINES="${LINE_PATH}/lines-women.txt"

CORES="${1:-4}"
STEPS="${2:-30}"

make main
eval "rm -rf ${OUTPUT_PATH}/*.png"
eval "mpirun --oversubscribe -np ${CORES} ./main ${IMG1} ${IMG2} ${OUTPUT_PATH}/ ${STEPS} ${LINES}"