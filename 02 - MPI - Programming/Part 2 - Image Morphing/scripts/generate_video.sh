INPUT_PATH="./out/images/"
OUTPUT_PATH="./out/videos/"
generate_video () {
    a=1
    for i in $( ls -1v ${INPUT_PATH}*.png  ); do
        new=$(printf "${INPUT_PATH}%03d.png" "$a")
        mv -i -- "$i" "$new"
        let a=a+1
    done
    ffmpeg -framerate 30 -loop 0 -i ${INPUT_PATH}%03d.png ${OUTPUT_PATH}output.gif 
}
generate_video
