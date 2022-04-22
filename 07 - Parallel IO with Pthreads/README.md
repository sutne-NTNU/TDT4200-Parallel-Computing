# Parallel I/O With PThreads

This just perform exactly the same as [Assignment 6](../06%20-%20CUDA%20-%20Image%20Morphing), but now the task is to parallelize the writing of the images to file with pthreads.

## Compile and run

From the root use
```-
./scripts/run.sh <steps>
```

## Result

Running with:

`./morph ./input/images/man9.jpg ./input/images/man10.jpg ./input/lines/lines-man9-man10.txt ./output/images/ 10`

or just

`./scripts/run.sh 10`

### Output

```-
./morph ./input/images/man9.jpg ./input/images/man10.jpg ./input/lines/lines-man9-man10.txt ./output/images/ 10

Loaded image from:      "./input/images/man9.jpg"
Loaded image from:      "./input/images/man10.jpg"
Loaded 33 lines from:   "./input/lines/lines-man9-man10.txt"
Morphing Images:        100%    |██████████████████████████████████████████████████| 11/11
Writing To File:
        Serial:         100%    |██████████████████████████████████████████████████| 11/11
        Time:   7.59 seconds
        pthreads:       100%    |██████████████████████████████████████████████████| 11/11
        Time:   3.79 seconds    (2.00 x Faster)
```

There seems to be a bottleneck when writing the images to file, regardless of how many images/steps (and therefore threads) the speedup is always around 2 times faster. I would expect it it to speedup by a factor closer to the number of images if all the images were written in parallel.
