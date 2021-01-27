#include <CL/sycl.hpp>
#include <dpct/dpct.hpp>
void kernel_layerforward(const float *input, float *input_weights,
                         float *hidden_partial_sum, const int hid,
                         sycl::nd_item<3> item_ct1, float *input_node,
                         float *weight_matrix)
{

  int by = item_ct1.get_group(1);
  int tx = item_ct1.get_local_id(2);
  int ty = item_ct1.get_local_id(1);

int index = ( hid + 1 ) * HEIGHT * by + ( hid + 1 ) * ty + tx + 1 + ( hid + 1 ) ;  

int index_in = HEIGHT * by + ty + 1;

if ( tx == 0 )
  input_node[ty] = input[index_in] ;
  item_ct1.barrier(sycl::access::fence_space::local_space);

weight_matrix[ty * WIDTH + tx] =  input_weights[index];
  item_ct1.barrier(sycl::access::fence_space::local_space);

weight_matrix[ty * WIDTH + tx]= weight_matrix[ty * WIDTH + tx] * input_node[ty];
  item_ct1.barrier(sycl::access::fence_space::local_space);

for ( int i = 1 ; i <= HEIGHT ; i=i*2){
  int power_two = i; 

  if( ty % power_two == 0 )
  weight_matrix[ty * WIDTH + tx]= weight_matrix[ty * WIDTH + tx] + weight_matrix[(ty + power_two/2)* WIDTH + tx];

    item_ct1.barrier(sycl::access::fence_space::local_space);
}

input_weights[index] =  weight_matrix[ty * WIDTH + tx];

  item_ct1.barrier(sycl::access::fence_space::local_space);

if ( tx == 0 ) {
  hidden_partial_sum[by * hid + ty] = weight_matrix[tx* WIDTH + ty];
}

}
