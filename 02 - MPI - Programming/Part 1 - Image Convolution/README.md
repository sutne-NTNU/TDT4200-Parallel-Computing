
# Part 1: Parallel image convolution using MPI

In this program, a convolutional kernel is shifted across an image in order to produce a new image. A kernel is an equation that determines the value of a pixel in the new image based on pixels in the old one. 

In words, for each color channel, the value in a new pixel is a sum of the corresponding old pixel and its neighbors. If we for the sake of example only look at a single color channel (say, red), then we’d multiply the ”here”-pixel’s red-value by 20, and then subtract 4 times the red-value of the orthogonal neighboring pixels and subtract 1 times the red-value of the diagonally neighboring pixels. The sum would be the new pixel’s red-value. This process is then repeated across all pixels and channels, and often across multiple iterations.

The challenge is that because of the shape of the kernels, each process needs the value of pixels outside it’s own partition. This is the problem you will have to solve by implementing what is called ***border exchange***.


##### Compilation

```sh
make
```

##### Execution

```sh
mpirun -np <NR_OF_PROCESSES> ./main [Options] <INPUT_IMAGE> <OUTPUT_IMAGE>    
```
Options:
```csv
-k, --kernel     <kernel>        kernel index (From 0 to 5)
-i, --iterations <iterations>    number of iterations 
```

**Example**
4 processes, running kernel 2 for 3 iterations:
```sh
mpirun -np 4 ./main -k 2 -i 3 images/input.jpeg images/output.png     
```

# Tasks

> All necessary changes should be doable only by adding code to the main() function. You can assume the number of processes are a power of 2.

1. Compile and run the code using at least 2 processes. Look at the resulting image. You should see a solid line on the border between the partitions.   

2. Fix the border exchange. Think about:
   
- Which pixels need to be sent to which processes?
- What order of communication is best to ensure quick and deadlock-free communication?
- Do the local image partitions have room to store received pixels? And do you need to make changes to scatter/gather if you change this?
  
3. Using `MPI Wtime`, measure how much time the program spends from start to finish using a single process (mpirun -np 1). Test with 2, 4 and 8 processes and document the ***speedup*** you get compared the single-process case.

# Results

## Input

Using this original image (3840 x 2160 pixels):

<img src="images/input.jpeg" width="100%"/>

Both output images are run with kernel 5 'Gaussian' which blurs the image. I run it with 8 processes for 32 iterations. To run with more processes than i have cores i use the `--oversubscribe` flag:
```
mpirun --oversubscribe -np 8 ./main -k 5 -i 32 images/input.jpeg images/output.png     
```

## Original Output 

<img src="images/original-output.bmp" width="100%"/>

## Output after completing the tasks

<img src="images/output.bmp" width="100%"/>

### Speedup

#### Running on Oppdal Server (4 cores)

```
Apply kernel 'Gaussian' on image with 3840 x 2160 pixels for 32 iterations
1 Processes used: 56.302354 seconds
2 Processes used: 28.335866 seconds = 1.989 Times Faster
4 Processes used: 14.943658 seconds = 3,768 Times Faster
8 Processes used: 15.473029 seconds = 3,639 Times Faster
```

#### Running on my computer (6 cores)

```
Apply kernel 'Gaussian' on image with 3840 x 2160 pixels for 32 iterations
1 Processes used: 54.159191 seconds 
2 Processes used: 26.984803 seconds = 2.007 Times Faster
4 Processes used: 14.737562 seconds = 3,674 Times Faster
8 Processes used: 11.186613 seconds = 4,842 Times Faster
```
