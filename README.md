# Description:
This project implements image processing FIR filters (high-pass and low-pass) and compares CPU and GPU performance using CUDA and Vulkan. It was developed as part of my thesis. Theses document(including additional performance metrics) is available [HERE](https://digikogu.taltech.ee/et/Download/d4f2879b-4da0-4b90-b29e-be158d28747e). Note that the document is in Estonian.

# Key Features / Skills Demonstrated:

* Image processing algorithms (FIR filters)

* Parallel computing and GPU acceleration

* C++ programming and performance optimization

* Data visualization of filter results and performance metrics

# How to run

To run the programs, first you must ensure that you have the followed software installed.

* Nvidia CUDA toolkit(Including GPU drivers)

* Vulkan SDK

* C++ compiler(MSVC, GCC, Lang, etc)

keep in mind that CUDA kernel also requires Nvidia GPU, while Vulkan will work on Nvidia, AMD and Intel GPU-s.

# Performance comparison

Here are some performance comparisons from the theses. To verify that the GPU computing pipeline was correctly implemented(for example that entire data was first loaded to GPU RAM), it was useful to compare the performance of the filter algorithm on CPU compared to GPU, as is shown on the first graph. The fact that GPU performance is so much faster, is a good sign that no basic mistakes were made on the GPU implementation of the algorithm.

![Alt text](performance_metrics/graph_CPU_vs_GPU.png)

To better compare how performance scales across tested GPU-s, Low pass filter was to be resizable. The larger the filter grid becomes, the more calculations GPU has to do per pixel on image. Graph below shows the amount of time various GPU-s took to filter the image as the filter size changed.

![Alt text](performance_metrics/graph_comparison_by_filter_size.png)

There are performance comparisons in the thesis document, including some related to amount of time it took to transfer data from one memory to another and back, however this was not analyzed as thoroughly as the pure computational aspect of applying filters to images.
