#include <stddef.h> // size_t

// check for duplicates in array
// O(sz^2) runtime 
void check_dupe(char* addresses, size_t sz);

// Check meta data for this particular allocation.
// Meta data consists of (size_t,void*) in this order.
//     size_t - size of allocation
//     void*  - start of block, NULL if free
void check_meta(char* addrs, size_t* vals, size_t index);

// Check the block which is associated with the address
// at index is still intact and correct.
// ie
//     Every allocation contains the correct number of
//     numbers set to the correct value, and the meta
//     data is correct for each allocation.
//     The block is still the correct size according
//     to meta data.
void check_block(char* addrs, size_t* vals, size_t index);

// Check that all the values in addrs and vals line up.
void check_all(char* addrs, size_t* vals, size_t arr_sz);
