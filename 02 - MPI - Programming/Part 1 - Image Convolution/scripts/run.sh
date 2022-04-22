CORES="${1:-1}"
ITERATIONS="${2:-32}"
KERNEL="${3:-5}"

IMAGE_PATH="./images"
INPUT_IMAGE="${IMAGE_PATH}/input.jpeg"
OUTPUT_IMAGE="${IMAGE_PATH}/output.bmp"

# Compile changes
make
# Execute
eval "mpirun --oversubscribe -np ${CORES} ./main  -i ${ITERATIONS} -k ${KERNEL} ${INPUT_IMAGE} ${OUTPUT_IMAGE}"
