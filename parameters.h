/*
 * Values can be provided by arguments in future versions
 */
#ifndef PARAMETERS_H_
#define PARAMETERS_H_

// Turn on logging
// #define LOG

// Number of hashtable buckets
#define BUCKET_NUM 32

// Number of total elemnts in the hashtable
#define ELEMENT_NUM 1000

// Size of hashtable element unused space (to test how the size affect the RDMA throughput and latency)
#define CHUNK 1

// Number of thread for clients
#define CLIENT_THREAD 1

// Number of thread for servers
#define SERVER_THREAD 1

// Total number of tests from client
#define TEST_NUM 50000

// Percentage of put operation from client
#define PUT_PERCENT 5

// Number of operations to calculate average statistics
#define STATISTICS_CYCLE 1000

#endif