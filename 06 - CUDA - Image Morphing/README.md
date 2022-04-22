# Image Morphing with CUDA

## Part 1
### Original Output with 10 steps
```-
Time in morphKernel (step 0): 3561.48 ms
Time in morphKernel (step 1): 3553.57 ms
Time in morphKernel (step 2): 3481.26 ms
Time in morphKernel (step 3): 3732.76 ms
Time in morphKernel (step 4): 3483.43 ms
Time in morphKernel (step 5): 3473.08 ms
Time in morphKernel (step 6): 3471.27 ms
Time in morphKernel (step 7): 3601.72 ms
Time in morphKernel (step 8): 3504.10 ms
Time in morphKernel (step 9): 3489.40 ms
Time in morphKernel (step 10): 3473.26 ms
Total time in GPU: 38825.74 ms
```


### Output when using CUDA

```-
Time in morphKernel (step 0): 7.42 ms
Time in morphKernel (step 1): 7.42 ms
Time in morphKernel (step 2): 7.42 ms
Time in morphKernel (step 3): 7.42 ms
Time in morphKernel (step 4): 7.42 ms
Time in morphKernel (step 5): 7.42 ms
Time in morphKernel (step 6): 7.42 ms
Time in morphKernel (step 7): 7.42 ms
Time in morphKernel (step 8): 7.42 ms
Time in morphKernel (step 9): 7.42 ms
Time in morphKernel (step 10): 7.42 ms
Total time in GPU: 109.27 ms
```

#### Speedup


|           | Average Step Time | Total GPU Time |
| :-------- | ----------------: | -------------: |
| Original  |        3 529.58ms |    38 825.74ms |
| With CUDA |            7.42ms |       109.27ms |
| Speedup   |      475 x faster |   355 x faster |


## Part 2

### Before using Shared Memory
```-
Section: Memory Workload Analysis
--------------------- --------------- ------------------------------
Memory Throughput        Gbyte/second                           2.64
Mem Busy                            %                          12.39
Max Bandwidth                       %                          21.12
L1/TEX Hit Rate                     %                          97.24
L2 Hit Rate                         %                          75.86
Mem Pipes Busy                      %                          21.12
--------------------- --------------- ------------------------------
```

### After using shared Memory
```-
Section: Memory Workload Analysis
--------------------- --------------- ------------------------------
Memory Throughput        Gbyte/second                           2.71
Mem Busy                            %                           8.42
Max Bandwidth                       %                           6.98
L1/TEX Hit Rate                     %                          93.20
L2 Hit Rate                         %                          76.96
Mem Pipes Busy                      %                           6.98
--------------------- --------------- ------------------------------
```
#### New Output
```-
Time in morphKernel (step 0): 7.18 ms
Time in morphKernel (step 1): 7.17 ms
Time in morphKernel (step 2): 7.17 ms
Time in morphKernel (step 3): 7.17 ms
Time in morphKernel (step 4): 7.17 ms
Time in morphKernel (step 5): 7.17 ms
Time in morphKernel (step 6): 7.17 ms
Time in morphKernel (step 7): 7.17 ms
Time in morphKernel (step 8): 7.17 ms
Time in morphKernel (step 9): 7.17 ms
Time in morphKernel (step 10): 7.17 ms
Total time in GPU: 107.16 ms
```

\clearpage
## Part 3

### Initial Occupancy Analysis
```-
Section: Occupancy
--------------------------------- --------------- -----------
Block Limit SM                              block          16
Block Limit Registers                       block          18
Block Limit Shared Mem                      block          36
Block Limit Warps                           block          16
Theoretical Active Warps per SM              warp          32
Theoretical Occupancy                           %         100
Achieved Occupancy                              %       97.26
Achieved Active Warps Per SM                 warp       31.12
--------------------------------- --------------- -----------
INF   This kernel's theoretical occupancy is not impacted by any block limit.    
```

### Analysis after using "Optimal Block Size"
```-
Section: Occupancy
--------------------------------- --------------- ------------
Block Limit SM                              block           16
Block Limit Registers                       block            1
Block Limit Shared Mem                      block           36
Block Limit Warps                           block            1
Theoretical Active Warps per SM              warp           32
Theoretical Occupancy                           %          100
Achieved Occupancy                              %        95.04
Achieved Active Warps Per SM                 warp        30.41
--------------------------------- --------------- ------------
INF   This kernel's theoretical occupancy is not impacted by any block limit.       
```

#### New Output

```-
Using:  blockSize = 32x32
        gridSize  = 32x32
Time in morphKernel (step 0): 8.88 ms
Time in morphKernel (step 1): 8.87 ms
Time in morphKernel (step 2): 8.87 ms
Time in morphKernel (step 3): 8.87 ms
Time in morphKernel (step 4): 8.87 ms
Time in morphKernel (step 5): 8.87 ms
Time in morphKernel (step 6): 8.87 ms
Time in morphKernel (step 7): 8.87 ms
Time in morphKernel (step 8): 8.87 ms
Time in morphKernel (step 9): 8.87 ms
Time in morphKernel (step 10): 8.87 ms
Total time in GPU: 133.87 ms
```


# Morphing Result

## Source
<p align="center">
    <img src="images/input/man9.jpg" width="50%">
</p>

## Destination
<p align="center">
    <img src="images/input/man10.jpg" width="50%">
</p>

## Output Gif
<p align="center">
    <img src="images/output/video/morph.gif" width="70%">
</p>

## Interesting result when indexing the lines wrong
<p align="center">
    <img src="images/output/video/daheck.png" width="50%">
</p>
