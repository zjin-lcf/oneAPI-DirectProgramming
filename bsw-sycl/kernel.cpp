#include "kernel.hpp"

  inline short
warpReduceMax_with_index(short val, short& myIndex, short& myIndex2, unsigned lengthSeqB, nd_item<1> &item)
{
  auto sg = item.get_sub_group();
  int   warpSize = 32;
  short myMax    = 0;
  short newInd   = 0;
  short newInd2  = 0;
  short ind      = myIndex;
  short ind2     = myIndex2;
  myMax          = val;
  // unsigned mask  = __ballot_sync(0xffffffff, threadIdx.x < lengthSeqB);  // blockDim.x
  // unsigned newmask;
  for(int offset = warpSize / 2; offset > 0; offset /= 2)
  {

    short tempVal = sg.shuffle_down(val, offset);
    val     = sycl::max(val,tempVal);
    newInd  = sg.shuffle_down(ind, offset);
    newInd2 = sg.shuffle_down(ind2, offset);
    if(val != myMax)
    {
      ind   = newInd;
      ind2  = newInd2;
      myMax = val;
    }
    else if(val == tempVal) // this is kind of redundant and has been done purely to match the results
      // with SSW to get the smallest alignment with highest score. Theoreticaly
      // all the alignmnts with same score are same.
    {
      if(newInd < ind){
        ind = newInd;
        ind2 = newInd2;
      }
    }
  }
  myIndex  = ind;
  myIndex2 = ind2;
  val      = myMax;
  return val;
}

  inline short
warpReduceMax_with_index_reverse(short val, short& myIndex, short& myIndex2, unsigned lengthSeqB, nd_item<1> &item)
{
  auto sg = item.get_sub_group();
  int   warpSize = 32;
  short myMax    = 0;
  short newInd   = 0;
  short newInd2  = 0;
  short ind      = myIndex;
  short ind2     = myIndex2;
  myMax          = val;
  for(int offset = warpSize / 2; offset > 0; offset /= 2)
  {

    short tempVal = sg.shuffle_down(val, offset);
    val     = sycl::max(val,tempVal);
    newInd  = sg.shuffle_down(ind, offset);
    newInd2 = sg.shuffle_down(ind2, offset);

    //  if(item.get_local_id(0) == 0)printf("index1:%d, index2:%d, max:%d\n", newInd, newInd2, val);
    if(val != myMax)
    {
      ind   = newInd;
      ind2  = newInd2;
      myMax = val;
    }
    else if(val == tempVal)  // this is kind of redundant and has been done purely to match the results
      // with SSW to get the smallest alignment with highest score. Theoreticaly
      // all the alignmnts with same score are same.
    {
      if(newInd2 > ind2){
        ind = newInd;
        ind2 = newInd2;

      }
    }
  }
  myIndex  = ind;
  myIndex2 = ind2;
  val      = myMax;
  return val;
}

  short
blockShuffleReduce_with_index_reverse(short myVal, short& myIndex, short& myIndex2, unsigned lengthSeqB, 
    short* locTots, short* locInds, short* locInds2, nd_item<1> &item)
{
  int              laneId = item.get_local_id(0) % 32;
  int              warpId = item.get_local_id(0) / 32;
  short            myInd  = myIndex;
  short            myInd2 = myIndex2;
  myVal                   = warpReduceMax_with_index_reverse(myVal, myInd, myInd2, lengthSeqB, item);

  item.barrier(access::fence_space::local_space);
  if(laneId == 0)
    locTots[warpId] = myVal;
  if(laneId == 0)
    locInds[warpId] = myInd;
  if(laneId == 0)
    locInds2[warpId] = myInd2;
  item.barrier(access::fence_space::local_space);
  unsigned check =
    ((32 + item.get_local_range(0) - 1) / 32);  // float check = ((float)item.get_local_range(0) / 32);
  if(item.get_local_id(0) < check)
  {
    myVal  = locTots[item.get_local_id(0)];
    myInd  = locInds[item.get_local_id(0)];
    myInd2 = locInds2[item.get_local_id(0)];
  }
  else
  {
    myVal  = 0;
    myInd  = -1;
    myInd2 = -1;
  }
  item.barrier(access::fence_space::local_space);

  if(warpId == 0)
  {
    myVal    = warpReduceMax_with_index_reverse(myVal, myInd, myInd2, lengthSeqB, item);
    myIndex  = myInd;
    myIndex2 = myInd2;
  }
  item.barrier(access::fence_space::local_space);
  return myVal;
}

  short
blockShuffleReduce_with_index(short myVal, short& myIndex, short& myIndex2, unsigned lengthSeqB, 
    short* locTots, short* locInds, short* locInds2, nd_item<1> &item)
{
  int              laneId = item.get_local_id(0) % 32;
  int              warpId = item.get_local_id(0) / 32;
  short            myInd  = myIndex;
  short            myInd2 = myIndex2;
  myVal                   = warpReduceMax_with_index(myVal, myInd, myInd2, lengthSeqB, item);

  item.barrier(access::fence_space::local_space);
  if(laneId == 0)
    locTots[warpId] = myVal;
  if(laneId == 0)
    locInds[warpId] = myInd;
  if(laneId == 0)
    locInds2[warpId] = myInd2;
  item.barrier(access::fence_space::local_space);
  unsigned check = ((32 + item.get_local_range(0) - 1) / 32); // float check = ((float)item.get_local_range(0) / 32);
  if(item.get_local_id(0) < check)
  {
    myVal  = locTots[item.get_local_id(0)];
    myInd  = locInds[item.get_local_id(0)];
    myInd2 = locInds2[item.get_local_id(0)];
  }
  else
  {
    myVal  = 0;
    myInd  = -1;
    myInd2 = -1;
  }
  item.barrier(access::fence_space::local_space);

  if(warpId == 0)
  {
    myVal    = warpReduceMax_with_index(myVal, myInd, myInd2, lengthSeqB, item);
    myIndex  = myInd;
    myIndex2 = myInd2;
  }
  item.barrier(access::fence_space::local_space);
  return myVal;
}



short findMaxFour(short first, short second, short third, short fourth)
{
  short maxScore = 0;

  maxScore = sycl::max(first,second);
  maxScore = sycl::max(maxScore, third);
  maxScore = sycl::max(maxScore, fourth);

  return maxScore;
}

void
sequence_aa_kernel(
    const char* seqA_array,
    const char* seqB_array,
    const unsigned* prefix_lengthA,
    const unsigned* prefix_lengthB,
    const short* seqA_align_begin,
          short* seqA_align_end,
    const short* seqB_align_begin,
          short* seqB_align_end,
          short* top_scores,
    const short startGap,
    const short extendGap,
    const short* scoring_matrix,
    const short* encoding_matrix,
    char* is_valid,
    short* sh_prev_E,
    short* sh_prev_H,
    short* sh_prev_prev_H,
    short* local_spill_prev_E,
    short* local_spill_prev_H,
    short* local_spill_prev_prev_H,
    short* sh_aa_encoding,
    short* sh_aa_scoring,
    short* locTots,
    short* locInds,
    short* locInds2,
    nd_item<1> &item)

{
  int block_Id  = item.get_group(0);
  int thread_Id = item.get_local_id(0);
  short laneId = item.get_local_id(0)%32;
  short warpId = item.get_local_id(0)/32;
  auto sg = item.get_sub_group();

  unsigned lengthSeqA;
  unsigned lengthSeqB;
  // local pointers
  const char* seqA;
  const char* seqB;
  const char* longer_seq;


  // setting up block local sequences and their lengths.
  if(block_Id == 0)
  {
    lengthSeqA = prefix_lengthA[0];
    lengthSeqB = prefix_lengthB[0];
    seqA       = seqA_array;
    seqB       = seqB_array;
  }
  else
  {
    lengthSeqA = prefix_lengthA[block_Id] - prefix_lengthA[block_Id - 1];
    lengthSeqB = prefix_lengthB[block_Id] - prefix_lengthB[block_Id - 1];
    seqA       = seqA_array + prefix_lengthA[block_Id - 1];
    seqB       = seqB_array + prefix_lengthB[block_Id - 1];
  }
  // what is the max length and what is the min length
  unsigned maxSize = lengthSeqA > lengthSeqB ? lengthSeqA : lengthSeqB;
  unsigned minSize = lengthSeqA < lengthSeqB ? lengthSeqA : lengthSeqB;

  // shared memory space for storing longer of the two strings
  if (thread_Id == 0) {
    memset(is_valid, 0, minSize);
    memset(is_valid+minSize, 1, minSize);
    memset(is_valid+2*minSize, 0, minSize);
  }
  is_valid += 2*minSize;

  char myColumnChar;
  // the shorter of the two strings is stored in thread registers
  if(lengthSeqA < lengthSeqB)
  {
    if(thread_Id < lengthSeqA)
      myColumnChar = seqA[thread_Id];  // read only once
    longer_seq = seqB;
  }
  else
  {
    if(thread_Id < lengthSeqB)
      myColumnChar = seqB[thread_Id];
    longer_seq = seqA;
  }

  item.barrier(access::fence_space::local_space); // this is required here so that complete sequence has been copied to shared memory

  int   i            = 1;
  short thread_max   = 0; // to maintain the thread max score
  short thread_max_i = 0; // to maintain the DP coordinate i for the longer string
  short thread_max_j = 0;// to maintain the DP cooirdinate j for the shorter string

  //initializing registers for storing diagonal values for three recent most diagonals (separate tables for
  //H, E and F)
  short _curr_H = 0, _curr_F = 0, _curr_E = 0;
  short _prev_H = 0, _prev_F = 0, _prev_E = 0;
  short _prev_prev_H = 0, _prev_prev_F = 0, _prev_prev_E = 0;
  short _temp_Val = 0;


  int max_threads = item.get_local_range(0);
  for(int p = thread_Id; p < SCORE_MAT_SIZE; p+=max_threads){
    sh_aa_scoring[p] = scoring_matrix[p];
  }
  for(int p = thread_Id; p < ENCOD_MAT_SIZE; p+=max_threads){
    sh_aa_encoding[p] = encoding_matrix[p];
  }

  item.barrier(access::fence_space::local_space); // to make sure all shmem allocations have been initialized

  for(int diag = 0; diag < lengthSeqA + lengthSeqB - 1; diag++)
  {  // iterate for the number of anti-diagonals

    is_valid = is_valid - (diag < minSize || diag >= maxSize); //move the pointer to left by 1 if cnd true

    _temp_Val = _prev_H; // value exchange happens here to setup registers for next iteration
    _prev_H = _curr_H;
    _curr_H = _prev_prev_H;
    _prev_prev_H = _temp_Val;
    _curr_H = 0;

    _temp_Val = _prev_E;
    _prev_E = _curr_E;
    _curr_E = _prev_prev_E;
    _prev_prev_E = _temp_Val;
    _curr_E = 0;

    _temp_Val = _prev_F;
    _prev_F = _curr_F;
    _curr_F = _prev_prev_F;
    _prev_prev_F = _temp_Val;
    _curr_F = 0;


    if(laneId == 31)
    { // if you are the last thread in your warp then spill your values to shmem
      sh_prev_E[warpId] = _prev_E;
      sh_prev_H[warpId] = _prev_H;
      sh_prev_prev_H[warpId] = _prev_prev_H;
    }

    if(diag >= maxSize)
    { // if you are invalid in this iteration, spill your values to shmem
      local_spill_prev_E[thread_Id] = _prev_E;
      local_spill_prev_H[thread_Id] = _prev_H;
      local_spill_prev_prev_H[thread_Id] = _prev_prev_H;
    }

    item.barrier(access::fence_space::local_space); // this is needed so that all the shmem writes are completed.

    if(is_valid[thread_Id] && thread_Id < minSize)
    {
      // unsigned mask  = __ballot_sync(__activemask(), (is_valid[thread_Id] &&( thread_Id < minSize)));

      short fVal = _prev_F + extendGap;
      short hfVal = _prev_H + startGap;
      short valeShfl = sg.shuffle(_prev_E, laneId- 1);
      short valheShfl = sg.shuffle(_prev_H, laneId - 1);

      short eVal=0, heVal = 0;

      if(diag >= maxSize) // when the previous thread has phased out, get value from shmem
      {
        eVal = local_spill_prev_E[thread_Id - 1] + extendGap;
        heVal = local_spill_prev_H[thread_Id - 1]+ startGap;
      }
      else
      {
        eVal =((warpId !=0 && laneId == 0)?sh_prev_E[warpId-1]: valeShfl) + extendGap;
        heVal =((warpId !=0 && laneId == 0)?sh_prev_H[warpId-1]:valheShfl) + startGap;
      }

      if(warpId == 0 && laneId == 0) // make sure that values for lane 0 in warp 0 is not undefined
      {
        eVal = 0;
        heVal = 0;
      }
      _curr_F = (fVal > hfVal) ? fVal : hfVal;
      _curr_E = (eVal > heVal) ? eVal : heVal;

      short testShufll = sg.shuffle(_prev_prev_H, laneId - 1);
      short final_prev_prev_H = 0;
      if(diag >= maxSize)
      {
        final_prev_prev_H = local_spill_prev_prev_H[thread_Id - 1];
      }
      else
      {
        final_prev_prev_H =(warpId !=0 && laneId == 0)?sh_prev_prev_H[warpId-1]:testShufll;
      }


      if(warpId == 0 && laneId == 0) final_prev_prev_H = 0;

      short mat_index_q = sh_aa_encoding[(int)longer_seq[i-1]];//encoding_matrix
      short mat_index_r = sh_aa_encoding[(int)myColumnChar];

      short add_score = sh_aa_scoring[mat_index_q*24 + mat_index_r]; // doesnt really matter in what order these indices are used, since the scoring table is symmetrical

      short diag_score = final_prev_prev_H + add_score;

      _curr_H = findMaxFour(diag_score, _curr_F, _curr_E, 0);
      thread_max_i = (thread_max >= _curr_H) ? thread_max_i : i;
      thread_max_j = (thread_max >= _curr_H) ? thread_max_j : thread_Id + 1;
      thread_max   = (thread_max >= _curr_H) ? thread_max : _curr_H;
      i++;
    }

    item.barrier(access::fence_space::local_space); // why do I need this? commenting it out breaks it

  }
  item.barrier(access::fence_space::local_space);

  thread_max = blockShuffleReduce_with_index(thread_max, thread_max_i, thread_max_j,
      minSize, locTots, locInds, locInds2, item);  // thread 0 will have the correct values


  if(thread_Id == 0)
  {


    if(lengthSeqA < lengthSeqB)
    {
      seqB_align_end[block_Id] = thread_max_i;
      seqA_align_end[block_Id] = thread_max_j;
      top_scores[block_Id] = thread_max;
    }
    else
    {
      seqA_align_end[block_Id] = thread_max_i;
      seqB_align_end[block_Id] = thread_max_j;
      top_scores[block_Id] = thread_max;
    }
  }
}

void sequence_aa_reverse_kernel(
    const char* seqA_array,
    const char* seqB_array,
    const unsigned* prefix_lengthA,
    const unsigned* prefix_lengthB,
          short* seqA_align_begin,
    const short* seqA_align_end,
          short* seqB_align_begin,
    const short* seqB_align_end,
    const short* top_scores,
    const short startGap,
    const short extendGap,
    const short* scoring_matrix,
    const short* encoding_matrix,
    char* is_valid,
    short* sh_prev_E,
    short* sh_prev_H,
    short* sh_prev_prev_H,
    short* local_spill_prev_E,
    short* local_spill_prev_H,
    short* local_spill_prev_prev_H,
    short* sh_aa_encoding,
    short* sh_aa_scoring,
    short* locTots, 
    short* locInds,
    short* locInds2,
    nd_item<1> &item)
{
  auto sg = item.get_sub_group();

  int block_Id  = item.get_group(0);
  int thread_Id = item.get_local_id(0);
  short laneId = item.get_local_id(0)%32;
  short warpId = item.get_local_id(0)/32;
  // local pointers
  const char* seqA;
  const char* seqB;
  const char* longer_seq;

  // setting up block local sequences and their lengths.
  if(block_Id == 0)
  {
    seqA       = seqA_array;
    seqB       = seqB_array;
  }
  else
  {
    seqA       = seqA_array + prefix_lengthA[block_Id - 1];
    seqB       = seqB_array + prefix_lengthB[block_Id - 1];
  }
  int newlengthSeqA = seqA_align_end[block_Id];
  int newlengthSeqB = seqB_align_end[block_Id];
  unsigned maxSize = newlengthSeqA > newlengthSeqB ? newlengthSeqA : newlengthSeqB;
  unsigned minSize = newlengthSeqA < newlengthSeqB ? newlengthSeqA : newlengthSeqB;
  char myColumnChar;

  if (thread_Id == 0) {
    memset(is_valid, 0, minSize);
    memset(is_valid+minSize, 1, minSize);
    memset(is_valid+2*minSize, 0, minSize);
  }
  is_valid += 2*minSize;

  //check if the new length of A is larger than B, if so then place the B string in registers and A in myLocString, make sure we dont do redundant copy by checking which string is located in myLocString before

  if(newlengthSeqA < newlengthSeqB)
  {
    if(thread_Id < newlengthSeqA)
    {
      myColumnChar = seqA[(newlengthSeqA - 1) - thread_Id];  // read only once
      longer_seq = seqB;
    }
  }
  else
  {
    if(thread_Id < newlengthSeqB)
    {
      myColumnChar = seqB[(newlengthSeqB - 1) - thread_Id];
      longer_seq = seqA;
    }
  }

  item.barrier(access::fence_space::local_space); // this is required  because sequence has been re-written in shmem

  int   i            = 1;
  short thread_max   = 0; // to maintain the thread max score
  short thread_max_i = 0; // to maintain the DP coordinate i for the longer string
  short thread_max_j = 0;// to maintain the DP cooirdinate j for the shorter string

  //initializing registers for storing diagonal values for three recent most diagonals (separate tables for
  //H, E and F)
  short _curr_H = 0, _curr_F = 0, _curr_E = 0;
  short _prev_H = 0, _prev_F = 0, _prev_E = 0;
  short _prev_prev_H = 0, _prev_prev_F = 0, _prev_prev_E = 0;
  short _temp_Val = 0;

  int max_threads = item.get_local_range(0);

  for(int p = thread_Id; p < SCORE_MAT_SIZE; p+=max_threads){
    sh_aa_scoring[p] = scoring_matrix[p];
  }
  for(int p = thread_Id; p < ENCOD_MAT_SIZE; p+=max_threads){
    sh_aa_encoding[p] = encoding_matrix[p];
  }


  item.barrier(access::fence_space::local_space); // this is required because shmem has been updated
  for(int diag = 0; diag <  newlengthSeqA + newlengthSeqB - 1; diag++)
  {
    is_valid = is_valid - (diag < minSize || diag >= maxSize);

    _temp_Val = _prev_H;
    _prev_H = _curr_H;
    _curr_H = _prev_prev_H;
    _prev_prev_H = _temp_Val;

    _curr_H = 0;


    _temp_Val = _prev_E;
    _prev_E = _curr_E;
    _curr_E = _prev_prev_E;
    _prev_prev_E = _temp_Val;
    _curr_E = 0;

    _temp_Val = _prev_F;
    _prev_F = _curr_F;
    _curr_F = _prev_prev_F;
    _prev_prev_F = _temp_Val;
    _curr_F = 0;

    if(laneId == 31)
    {
      sh_prev_E[warpId] = _prev_E;
      sh_prev_H[warpId] = _prev_H;
      sh_prev_prev_H[warpId] = _prev_prev_H;
    }

    if(diag>= maxSize)
    { // if you are invalid in this iteration, spill your values to shmem
      local_spill_prev_E[thread_Id] = _prev_E;
      local_spill_prev_H[thread_Id] = _prev_H;
      local_spill_prev_prev_H[thread_Id] = _prev_prev_H;
    }

    item.barrier(access::fence_space::local_space);

    if(is_valid[thread_Id] && thread_Id < minSize)
    {

      //unsigned mask  = __ballot_sync(__activemask(), (is_valid[thread_Id] &&( thread_Id < minSize)));

      short fVal = _prev_F + extendGap;
      short hfVal = _prev_H + startGap;
      short valeShfl = sg.shuffle(_prev_E, laneId- 1);
      short valheShfl = sg.shuffle(_prev_H, laneId - 1);

      short eVal=0;
      short heVal = 0;

      if(diag >= maxSize)
      {
        eVal = local_spill_prev_E[thread_Id - 1] + extendGap;
      }
      else
      {
        eVal =((warpId !=0 && laneId == 0)?sh_prev_E[warpId-1]: valeShfl) + extendGap;
      }

      if(diag >= maxSize)
      {
        heVal = local_spill_prev_H[thread_Id - 1]+ startGap;
      }
      else
      {
        heVal =((warpId !=0 && laneId == 0)?sh_prev_H[warpId-1]:valheShfl) + startGap;
      }

      if(warpId == 0 && laneId == 0)
      {
        eVal = 0;
        heVal = 0;
      }
      _curr_F = (fVal > hfVal) ? fVal : hfVal;
      _curr_E = (eVal > heVal) ? eVal : heVal;
      short testShufll = sg.shuffle(_prev_prev_H, laneId - 1);
      short final_prev_prev_H =0;

      if(diag >= maxSize)
      {
        final_prev_prev_H = local_spill_prev_prev_H[thread_Id - 1];
      }
      else
      {
        final_prev_prev_H =(warpId !=0 && laneId == 0)?sh_prev_prev_H[warpId-1]:testShufll;
      }

      if(warpId == 0 && laneId == 0) final_prev_prev_H = 0;

      short mat_index_q = sh_aa_encoding[(int)longer_seq[maxSize-i]];
      short mat_index_r = sh_aa_encoding[(int)myColumnChar];
      short add_score = sh_aa_scoring[mat_index_q*24 + mat_index_r]; // doesnt really matter in what order these indices are used, since the scoring table is symmetrical

      short diag_score = final_prev_prev_H + add_score;
      _curr_H = findMaxFour(diag_score, _curr_F, _curr_E, 0);

      thread_max_i = (thread_max >= _curr_H) ? thread_max_i : maxSize - i ;//i;// begin_A (longer string)
      thread_max_j = (thread_max >= _curr_H) ? thread_max_j : minSize - thread_Id -1; // begin_B (shorter string)
      thread_max   = (thread_max >= _curr_H) ? thread_max : _curr_H;
      i++;

    }
    item.barrier(access::fence_space::local_space);

  }
  item.barrier(access::fence_space::local_space);




  thread_max = blockShuffleReduce_with_index_reverse(thread_max, thread_max_i, thread_max_j,
      minSize, locTots, locInds, locInds2, item );  // thread 0 will have the correct values
  if(thread_Id == 0)
  {
    if(newlengthSeqA < newlengthSeqB)
    {
      seqB_align_begin[block_Id] = /*newlengthSeqB - */(thread_max_i);
      seqA_align_begin[block_Id] = /*newlengthSeqA */ (thread_max_j);
    }
    else
    {
      seqA_align_begin[block_Id] = /*newlengthSeqA - */(thread_max_i);
      seqB_align_begin[block_Id] = /*newlengthSeqB -*/ (thread_max_j);
    }
  }
  item.barrier(access::fence_space::local_space);

}
