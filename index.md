# Introduction

<div>
    <p align="justify">
    Image/Video editing tools and applications have been widely used in many areas, including marketing, fashion design, and film production. Among these applications, we are especially interested in Image Completion, and think it is an important function. Why is Image Completion important? Actually, it is very useful for lots of purposes. No matter it’s for everyday photo or movie post-processing, it can help you remove objects you don’t want (see Fig1 and Fig2).
    </p>
</div>

<div align="center">
    <div class="img-with-text">
        <img src="report/image/image1.png" class="inline" width="667" height="220"/>
        <p><b>Figure 1:</b> <i>Everyday Photo</i></p>
        <img src="report/image/image2.png" class="inline" width="554" height="315"/>
        <p><b>Figure 2:</b> <i>Movie Post-Processing</i></p>
    </div>
</div>

# State-of-the-Art

<div>
    <p align="justify">
    We survey the literature works. The latest one is from SIGGRAPH 2017 (see Fig3), which is based on  deep neural network. 
    </p>
</div>

<div align="center">
    <div class="img-with-text">
        <img src="report/image/image3.png" class="inline" width="775" height="266"/>
        <p><b>Figure 3:</b> <i>Deep convolutional neural networks</i> [1]</p>
    </div>
</div>

<div>
    <p align="justify">
    Although this work is very powerful, it may still fail in some cases (see Fig4). We also find other problems with this model. Usually, It takes lots of time and images to train a model. Also, it doesn’t provide an interactive way for users to improve results if they are not good enough.
    </p>
</div>

<div align="center">
    <div class="img-with-text">
        <img src="report/image/image4.png" class="inline" width="775" height="266"/>
        <p><b>Figure 4:</b> <i>Failed case in</i> [1]</p>
    </div>
</div>

# Adobe Photoshop: Content Aware Fill (PatchMatch)

<div>
    <p align="justify">
    Thus, we’re more interested in what’s the industry solution for this problem since we usually ask for a higher standard of image quality provided by those market products. We find that Adobe Photoshop also has this function which is called Content Aware Fill (see Fig5) using PatchMatch algorithm [2]. So, we try to find some insights from this algorithm.
    </p>
</div>

<div align="center">
    <div class="img-with-text">
        <img src="report/image/image5.png" class="inline" width="914" height="442"/>
        <p><b>Figure 5:</b> <i>Adobe Photoshop: Content Aware Fill</i></p>
    </div>
</div>

# PatchMatch 
## Algorithm

<div>
    <p align="justify">
    The purpose of the PatchMatch algorithm is to efficiently find the similar patches between two images. Basically, its algorithm contains three steps: random initlization, propagation, and random search (see Fig6).
    </p>
</div>

<div align="center">
    <div class="img-with-text">
        <img src="report/image/image6.png" class="inline" width="620" height="426"/>
        <p><b>Figure 6:</b> <i>PatchMatch Algorithm</i></p>
    </div>
</div>

<div>
    <p align="justify">
    Take Fig7 as example. For each patch in A, we try to find a similar patch in B. In the fist step, we randomly match each patch in A with a patch in B. If we’re lucky, we may get some pairs already similar enough. Then, in the second step, we check if our neighbors can give us a better candidate patch. If it is, we just propagate this benefit, i.e., just update the patch. In the final step, we continue looking for a better patch in our neighborhood by random search in order to get out of a local optimum.
    </p>
</div>

<div align="center">
    <div class="img-with-text">
        <img src="report/image/image7.png" class="inline" width="947" height="465"/>
        <p><b>Figure 7:</b> <i>Example of PatchMatch Algorithm</i></p>
    </div>
</div>

## Simulation Result

# Image Completion Algorithm
# Interactive GUI
# Simulation Results
# Future Work
1. Adopt KNN rather than the most similar patch
2. Figure out a way for better initialization
3. Find how to determine patch size
4. Speed up our application to real time
5. Improve the sensitivity issues for initialization and patch size

# Presentation
[Slides](https://docs.google.com/presentation/d/1YMoPjG7pNWLSEDjNDadcxzCkqFpysvW_enfNe-DhoVM/edit#slide=id.p1)

# Reference
1. S. Iizuka, E. Simo-Serra, and H. Ishikawa. Globally and Locally Consistent Image Completion. ACM Transactions on Graphics (Proc. of SIGGRAPH 2017), 36(4):107:1–107:14, 2017.
2. C. Barnes, E. Shechtman, A. Finkelstein, and D. Goldman. Patchmatch: a randomized correspondence algorithm for structural image editing. TOG, 2009.

## Tmp
1. [Original documents](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage1/documents/original)
2. [Documents with markup](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage1/documents/marked)<br>- [README](https://github.com/WenFuLee/CS-839-Data-Science/blob/master/stage1/documents/marked/README): the selected entity type
3. [Documents in set I](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage1/documents/set_I)
4. [Documents in set J](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage1/documents/set_J)
5. [Code](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage1/code)
6. [Compressed file, including all the files above](https://github.com/WenFuLee/CS-839-Data-Science/blob/master/stage1/compressed_files.zip)
7. [PDF Report](https://github.com/WenFuLee/CS-839-Data-Science/blob/master/stage1/Project%20Stage%201_Report.pdf)

1. [Data](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage2/data)
2. [Code](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage2/code)
3. [PDF Report](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage2/stage2_report.pdf)


1. [Data](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage3/data)<br>- Please check README to see what files to look at
2. [Code](https://github.com/WenFuLee/CS-839-Data-Science/tree/master/stage3/code)<br>- Please check README to see what files to look at
3. [PDF Report](https://github.com/WenFuLee/CS-839-Data-Science/blob/master/stage3/Project%20Stage%203_Report.pdf)
