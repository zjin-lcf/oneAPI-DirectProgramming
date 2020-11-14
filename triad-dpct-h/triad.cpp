#define DPCT_USM_LEVEL_NONE
#include <CL/sycl.hpp>
#include <dpct/dpct.hpp>

#include "OptionParser.h"
#include "Timer.h"
#include "Utility.h"

// ****************************************************************************
// Function: addBenchmarkSpecOptions
//
// Purpose:
//   Add benchmark specific options parsing
//
// Arguments:
//   op: the options parser / parameter database
//
// Returns:  nothing
//
// Programmer: Kyle Spafford
// Creation: December 15, 2009
//
// Modifications:
//
// ****************************************************************************
void addBenchmarkSpecOptions(OptionParser &op)
{
  ;
}

// ****************************************************************************
// Function: triad
//
// Purpose:
//   A simple vector addition kernel
//   C = A + s*B
//
// Arguments:
//   A,B - input vectors
//   C - output vectors
//   s - scalar
//
// Returns:  nothing
//
// Programmer: Kyle Spafford
// Creation: December 15, 2009
//
// Modifications:
//
// ****************************************************************************
void triad(float* A, float* B, float* C, float s, sycl::nd_item<3> item_ct1)
{
   int gid = item_ct1.get_local_id(2) +
             (item_ct1.get_group(2) * item_ct1.get_local_range().get(2));
  C[gid] = A[gid] + s*B[gid];
}

// ****************************************************************************
// Function: RunBenchmark
//
// Purpose:
//   Implements the Stream Triad benchmark in CUDA.  This benchmark
//   is designed to test CUDA's overall data transfer speed. It executes
//   a vector addition operation with no temporal reuse. Data is read
//   directly from the global memory. This implementation tiles the input
//   array and pipelines the vector addition computation with
//   the data download for the next tile. However, since data transfer from
//   host to device is much more expensive than the simple vector computation,
//   data transfer operations should completely dominate the execution time.
//
// Arguments:
//   resultDB: results from the benchmark are stored in this db
//   op: the options parser (contains input parameters)
//
// Returns:  nothing
//
// Programmer: Kyle Spafford
// Creation: December 15, 2009
//
// Modifications:
//
// ****************************************************************************
void RunBenchmark(OptionParser &op)
{
   dpct::device_ext &dev_ct1 = dpct::get_current_device();
  const bool verbose = op.getOptionBool("verbose");
  const int n_passes = op.getOptionInt("passes");

  // 256k through 8M bytes
  const int nSizes = 9;
  const size_t blockSizes[] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192,
    16384 };
  const size_t memSize = 16384;
  const size_t numMaxFloats = 1024 * memSize / 4;
  const size_t halfNumFloats = numMaxFloats / 2;

  // Create some host memory pattern
  srand48(8650341L);
  float *h_mem;
   h_mem = (float *)malloc(sizeof(float) * numMaxFloats);

  // Allocate some device memory
  float* d_memA0, *d_memB0, *d_memC0;
   dpct::dpct_malloc((void **)&d_memA0, blockSizes[nSizes - 1] * 1024);
   dpct::dpct_malloc((void **)&d_memB0, blockSizes[nSizes - 1] * 1024);
   dpct::dpct_malloc((void **)&d_memC0, blockSizes[nSizes - 1] * 1024);

  float* d_memA1, *d_memB1, *d_memC1;
   dpct::dpct_malloc((void **)&d_memA1, blockSizes[nSizes - 1] * 1024);
   dpct::dpct_malloc((void **)&d_memB1, blockSizes[nSizes - 1] * 1024);
   dpct::dpct_malloc((void **)&d_memC1, blockSizes[nSizes - 1] * 1024);

  float scalar = 1.75f;

  const size_t blockSize = 128;

  // Number of passes. Use a large number for stress testing.
  // A small value is sufficient for computing sustained performance.
  for (int pass = 0; pass < n_passes; ++pass)
  {
    // Step through sizes forward
    for (int i = 0; i < nSizes; ++i)
    {
      int elemsInBlock = blockSizes[i] * 1024 / sizeof(float);
      for (int j = 0; j < halfNumFloats; ++j)
        h_mem[j] = h_mem[halfNumFloats + j]
          = (float) (drand48() * 10.0);

      // Copy input memory to the device
      if (verbose) {
        cout << ">> Executing Triad with vectors of length "
          << numMaxFloats << " and block size of "
          << elemsInBlock << " elements." << "\n";
        printf("Block:%05ldKB\n", blockSizes[i]);
      }

      // start submitting blocks of data of size elemsInBlock
      // overlap the computation of one block with the data
      // download for the next block and the results upload for
      // the previous block
      int crtIdx = 0;
      size_t globalWorkSize = elemsInBlock / blockSize;

         sycl::queue *streams[2];
         streams[0] = dev_ct1.create_queue();
         streams[1] = dev_ct1.create_queue();

      int TH = Timer::Start();

         dpct::async_dpct_memcpy(d_memA0, h_mem, blockSizes[i] * 1024,
                                 dpct::host_to_device, *(streams[0]));
         dpct::async_dpct_memcpy(d_memB0, h_mem, blockSizes[i] * 1024,
                                 dpct::host_to_device, *(streams[0]));

         {
            dpct::buffer_t d_memA0_buf_ct0 = dpct::get_buffer(d_memA0);
            dpct::buffer_t d_memB0_buf_ct1 = dpct::get_buffer(d_memB0);
            dpct::buffer_t d_memC0_buf_ct2 = dpct::get_buffer(d_memC0);
            streams[0]->submit([&](sycl::handler &cgh) {
               auto d_memA0_acc_ct0 =
                   d_memA0_buf_ct0.get_access<sycl::access::mode::read_write>(
                       cgh);
               auto d_memB0_acc_ct1 =
                   d_memB0_buf_ct1.get_access<sycl::access::mode::read_write>(
                       cgh);
               auto d_memC0_acc_ct2 =
                   d_memC0_buf_ct2.get_access<sycl::access::mode::read_write>(
                       cgh);

               cgh.parallel_for(
                   sycl::nd_range<3>(sycl::range<3>(1, 1, globalWorkSize) *
                                         sycl::range<3>(1, 1, blockSize),
                                     sycl::range<3>(1, 1, blockSize)),
                   [=](sycl::nd_item<3> item_ct1) {
                      triad((float *)(&d_memA0_acc_ct0[0]),
                            (float *)(&d_memB0_acc_ct1[0]),
                            (float *)(&d_memC0_acc_ct2[0]), scalar, item_ct1);
                   });
            });
         }

      if (elemsInBlock < numMaxFloats)
      {
        // start downloading data for next block
            dpct::async_dpct_memcpy(d_memA1, h_mem + elemsInBlock,
                                    blockSizes[i] * 1024, dpct::host_to_device,
                                    *(streams[1]));
            dpct::async_dpct_memcpy(d_memB1, h_mem + elemsInBlock,
                                    blockSizes[i] * 1024, dpct::host_to_device,
                                    *(streams[1]));
      }

      int blockIdx = 1;
      unsigned int currStream = 1;
      while (crtIdx < numMaxFloats)
      {
        currStream = blockIdx & 1;
        // Start copying back the answer from the last kernel
        if (currStream)
        {
               dpct::async_dpct_memcpy(h_mem + crtIdx, d_memC0,
                                       elemsInBlock * sizeof(float),
                                       dpct::device_to_host, *(streams[0]));
        }
        else
        {
               dpct::async_dpct_memcpy(h_mem + crtIdx, d_memC1,
                                       elemsInBlock * sizeof(float),
                                       dpct::device_to_host, *(streams[1]));
        }

        crtIdx += elemsInBlock;

        if (crtIdx < numMaxFloats)
        {
          // Execute the kernel
          if (currStream)
          {
                  dpct::buffer_t d_memA1_buf_ct0 = dpct::get_buffer(d_memA1);
                  dpct::buffer_t d_memB1_buf_ct1 = dpct::get_buffer(d_memB1);
                  dpct::buffer_t d_memC1_buf_ct2 = dpct::get_buffer(d_memC1);
                  streams[1]->submit([&](sycl::handler &cgh) {
                     auto d_memA1_acc_ct0 =
                         d_memA1_buf_ct0
                             .get_access<sycl::access::mode::read_write>(cgh);
                     auto d_memB1_acc_ct1 =
                         d_memB1_buf_ct1
                             .get_access<sycl::access::mode::read_write>(cgh);
                     auto d_memC1_acc_ct2 =
                         d_memC1_buf_ct2
                             .get_access<sycl::access::mode::read_write>(cgh);

                     cgh.parallel_for(sycl::nd_range<3>(
                                          sycl::range<3>(1, 1, globalWorkSize) *
                                              sycl::range<3>(1, 1, blockSize),
                                          sycl::range<3>(1, 1, blockSize)),
                                      [=](sycl::nd_item<3> item_ct1) {
                                         triad((float *)(&d_memA1_acc_ct0[0]),
                                               (float *)(&d_memB1_acc_ct1[0]),
                                               (float *)(&d_memC1_acc_ct2[0]),
                                               scalar, item_ct1);
                                      });
                  });
          }
          else
          {
                  dpct::buffer_t d_memA0_buf_ct0 = dpct::get_buffer(d_memA0);
                  dpct::buffer_t d_memB0_buf_ct1 = dpct::get_buffer(d_memB0);
                  dpct::buffer_t d_memC0_buf_ct2 = dpct::get_buffer(d_memC0);
                  streams[0]->submit([&](sycl::handler &cgh) {
                     auto d_memA0_acc_ct0 =
                         d_memA0_buf_ct0
                             .get_access<sycl::access::mode::read_write>(cgh);
                     auto d_memB0_acc_ct1 =
                         d_memB0_buf_ct1
                             .get_access<sycl::access::mode::read_write>(cgh);
                     auto d_memC0_acc_ct2 =
                         d_memC0_buf_ct2
                             .get_access<sycl::access::mode::read_write>(cgh);

                     cgh.parallel_for(sycl::nd_range<3>(
                                          sycl::range<3>(1, 1, globalWorkSize) *
                                              sycl::range<3>(1, 1, blockSize),
                                          sycl::range<3>(1, 1, blockSize)),
                                      [=](sycl::nd_item<3> item_ct1) {
                                         triad((float *)(&d_memA0_acc_ct0[0]),
                                               (float *)(&d_memB0_acc_ct1[0]),
                                               (float *)(&d_memC0_acc_ct2[0]),
                                               scalar, item_ct1);
                                      });
                  });
          }
        }

        if (crtIdx+elemsInBlock < numMaxFloats)
        {
          // Download data for next block
          if (currStream)
          {
                  dpct::async_dpct_memcpy(d_memA0,
                                          h_mem + crtIdx + elemsInBlock,
                                          blockSizes[i] * 1024,
                                          dpct::host_to_device, *(streams[0]));
                  dpct::async_dpct_memcpy(d_memB0,
                                          h_mem + crtIdx + elemsInBlock,
                                          blockSizes[i] * 1024,
                                          dpct::host_to_device, *(streams[0]));
          }
          else
          {
                  dpct::async_dpct_memcpy(d_memA1,
                                          h_mem + crtIdx + elemsInBlock,
                                          blockSizes[i] * 1024,
                                          dpct::host_to_device, *(streams[1]));
                  dpct::async_dpct_memcpy(d_memB1,
                                          h_mem + crtIdx + elemsInBlock,
                                          blockSizes[i] * 1024,
                                          dpct::host_to_device, *(streams[1]));
          }
        }
        blockIdx += 1;
        currStream = !currStream;
      }

         dev_ct1.queues_wait_and_throw();
      double time = Timer::Stop(TH, "thread synchronize");

      double triad = ((double)numMaxFloats * 2.0) / (time*1e9);
      if (verbose)
        std::cout << "TriadFlops " << triad << " GFLOPS/s\n";

      double bdwth = ((double)numMaxFloats*sizeof(float)*3.0)
        / (time*1000.*1000.*1000.);
      if (verbose)
        std::cout << "TriadBdwth " << bdwth << " GB/s\n";


      // Checking memory for correctness. The two halves of the array
      // should have the same results.
      if (verbose) cout << ">> checking memory\n";
      for (int j=0; j<halfNumFloats; ++j)
      {
        if (h_mem[j] != h_mem[j+halfNumFloats])
        {
          cout << "Error; hostMem[" << j << "]=" << h_mem[j]
            << " is different from its twin element hostMem["
            << (j+halfNumFloats) << "]: "
            << h_mem[j+halfNumFloats] << "stopping check\n";
          break;
        }
      }
      if (verbose) cout << ">> finish!" << endl;

      // Zero out the test host memory
      for (int j=0; j<numMaxFloats; ++j)
        h_mem[j] = 0.0f;
    }
  }

  // Cleanup
   dpct::dpct_free(d_memA0);
   dpct::dpct_free(d_memB0);
   dpct::dpct_free(d_memC0);
   dpct::dpct_free(d_memA1);
   dpct::dpct_free(d_memB1);
   dpct::dpct_free(d_memC1);
   free(h_mem);
}
