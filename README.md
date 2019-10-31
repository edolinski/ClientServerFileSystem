# Client Server File System

The Client Server File System, as the name suggests, is designed for running a remote file system on a server made available to clients over a network. Clients interact with the server over a persistent TCP connection through a request response protocol with commands for reading and writing files in the file system. While reads of files can be fulfilled with a single request and response, writes are more involved. Before a client can begin to write a file, the client must request a new transaction for the file it wishes to write from the server. Transactions help the server keep track of independent sets of `WRITE` requests so the consistency of the file system can be preserved when the client commits `WRITE` requests to the server's disk. After the client has requested a new transaction (specifying the associated file as payload) and has obtained the unique transaction id from the server, the client can begin sending `WRITE` requests. Each `WRITE` request contains among other things, the transaction id and the data to be written to the file as well as a sequence number. The sequence number is used to specify the relative order of a series of `WRITE` requests. For example, a `WRITE` request with a sequence number of 5 ensures this will be the 5th `WRITE` request (as part of a transaction with 5 or more `WRITE` requests) committed to disk when the client commits. Such a mechanism allows the client to send `WRITE` requests out of order knowing they will be written in the correct order when committed to disk. At the point the client is done sending `WRITE` requests for a given transaction, the client sends a `COMMIT` request to commit the `WRITE` requests to the server’s disk. As a side note, if the client attempts to commit before all `WRITE` requests up to the highest sequenced numbered `WRITE` request have been received, the server will ask the client to resend `WRITE` requests for missing sequence numbers and will require a subsequent `COMMIT` request to commit `WRITE` requests to disk.

The server processes client requests concurrently and in parallel with transactional semantics (ACID). For example, if more than one transaction is associated with the same file, the server ensures `WRITE` requests from separate transactions are not interleaved when committing to disk. Alternatively, if more than one client is interacting with the same transaction simultaneously, the server ensures Atomicity (A), Consistency (C), and Isolation (I) in the face of competing writes, commits, and aborts. As for Durability (D), the server logs transactions as they are created, committed, aborted, or timed out, so the server can reconstruct the last valid state of the file system prior to a system crash or power failure. This includes rolling back any writes flushed to disk as part of an incomplete transaction.

Lastly, the server will abort any transaction for which no request has been received within transaction_timeout_seconds (please see constants.h). Please note a connection will timeout according to a separate timeout parameter, connection_timeout_seconds, if no packet has been received by the server in this time.

## Wire Protocol

### Request format:

| COMMAND | TXN_ID | SEQ_NUM | CONTENT_LEN | DATA |
| ---     | ---    | ---     | ---         | ---  |

* __COMMAND__ – Used to indicate the type of operation.
* __TXN_ID__ – Used to identify the transaction the request pertains to. Must be set to `-1` for `NEW_TXN` requests.
* __SEQ_NUM__ – Used to specify the order of `WRITE` requests within a transaction. Must be set to `0` for `NEW_TXN` request and \> 0 for subsequent `WRITE` requests.
* __CONTENT_LEN__ – Used to indicate the number of bytes in the __DATA__ field.
* __DATA__ – Used to hold the data to be written on `WRITE` requests and the name of the file on `NEW_TXN` requests.

### Response format:

| COMMAND | TXN_ID | SEQ_NUM | ERROR_CODE | CONTENT_LEN | DATA |
| ---     | ---    | ---     | ---        | ---         | ---  |

* __COMMAND__ – Used to indicate the type of operation.
* __TXN_ID__ – Used to identify the transaction the response pertains to.
* __SEQ_NUM__ – Used to acknowledge individual `WRITE` requests or to indicate a `WRITE` request that needs to be resent.
* __ERROR_CODE__ – Used to indicate what type of error has occurred if applicable.
* __CONTENT_LEN__ – Used to indicate the number of bytes in the __DATA__ field.
* __DATA__ – Used to hold the file contents in response to a successful `READ` request or a description of an error if applicable.

### Request Response Header:

* For both the request and response formats all but the __DATA__ field make up the header, with each field delimited by a space character. A request header is 64 bytes long while a response header is 128 bytes long.

### Commands:

* __Request Commands:__

 * `ABORT` – Used to abort the transaction specified under __TXN_ID__.
 * `COMMIT` – Used to commit `WRITE` requests received as part of the transaction specified under __TXN_ID__ with __SEQ_NUM__ indicating the highest sequence numbered `WRITE` request sent to the server. If any `WRITE` request up this sequence number has not been received by the server, the commit will fail.
 * `NEW_TXN` – Used to create a new transaction on the server. To be successful, __SEQ_NUM__ must be set to `0` and __DATA__ must be set to the name of the file the transaction pertains to.
 * `READ` – Used to read a particular file on the server. To be successful, __DATA__ must be set to the name of a file on the server.
 * `WRITE` – Used to add __DATA__ (to be written on `COMMIT`) to the transaction specified under __TXN_ID__. Each `WRITE` request must also specify a __SEQ_NUM__, k > 0 (0 reserved for `NEW_TXN`), so the server knows to commit the `WRITE` request kth overall when committing the transaction.

* __Response Commands:__

 * `ACK` – Used to acknowledge a successful `ABORT`, `COMMIT`, `NEW_TXN`, or `WRITE` operation. The server in response to a `NEW_TXN` operation will include in the `ACK` the __TXN_ID__ corresponding to the newly created transaction.
 * `ASK_RESEND` – Used to ask the client to resend the `WRITE` request corresponding to transaction __TXN_ID__ with sequence number __SEQ_NUM__ . This response will be sent when the client attempts to `COMMIT` before the server has received all `WRITE` requests up to __SEQ_NUM__ in `COMMIT`.
 * `ERROR` – Used to indicate an error with error code __ERROR_CODE__ has occurred for the transaction specified under __TXN_ID__.

### Error Codes:

* Please see errors.h under ClientServerShared.

### Examples:

* __Example__ `COMMIT` __Request/Response Sequence:__

 Client: `NEW_TXN` `-1` `0` `12` `new_file.txt`

 Server: `ACK` `12345` `0` `0` `0`

 Client: `WRITE` `12345` `1` `56` `Here is my data for transaction 12345 to write on commit`

 Server: `ACK` `12345` `1` `0` `0`

 Client: `COMMIT` `12345` `1` `0`

 Server: `ACK` `12345` `1` `0` `0`

* __Example__ `READ` __Request/Response Sequence:__

  Client: `READ` `-1` `-1` `11` `new_file.txt`

  Server: `ACK` `-1` `-1` `0` `56` `Here is my data for transaction 12345 to write on commit`

## Build Instructions

Tested with Xcode Version 9.4.1 (9F2000)

1. Download or clone Google Test from https://github.com/google/googletest/tree/v1.8.x

2. Navigate to googletest-1.8.x/googletest/xcode and open and build gtest.xcodeproj in Xcode or use the prebuilt gtest framework provided under GoogleTestFramework directory

__With Environment Variables:__

3. Set the following environment variables:

 GTEST_DIR – set to \<path_to_dir_containing_googletest-1.8.x\>/googletest-1.8.x/googletest

 GTEST_FRAMEWORK_DIR – set to \<path_to_dir_containing_gtest.framework\> built in step 2

 CLIENT_SERVER_SHARED_DIR – set to \<path_to_ClientServerShared_dir\>

 > For help launching Xcode with environment variables please see: https://stackoverflow.com/questions/33235958/xcode-doesnt-recognize-environment-variables

4. Navigate to ClientServerTest and open and build ClientServerTest.xcodeproj in Xcode

5. Set the product environment variable DYLD_FRAMEWORK_PATH to $GTEST_FRAMEWORK_DIR – product environment variables can be added in Xcode by navigating to Product -> Scheme -> Edit Scheme -> Run (in left column) -> Arguments -> Environment Variables

6. Navigate to Server and open and build Server.xcodeproj in Xcode

7. You can now run ClientServerTest* and Server from Xcode or command line

\* Note: To run client server tests from command line DYLD_FRAMEWORK_PATH environment variable needs to be set to $GTEST_FRAMEWORK_DIR  

__Without Environment Variables:__

3. Navigate to ClientServerTest and open ClientServerTest.xcodeproj in Xcode

4. Under Build Settings -> Search Paths -> Framework Search Paths, add path to directory containing gtest.framework built in step 2

5. Under Build Settings -> Search Paths -> Header Search Paths, add path to googletest-1.8.x/googletest/include and path to ClientServerShared directory

6. Set the product environment variable DYLD_FRAMEWORK_PATH to the path from step 4 (i.e. the path to directory containing gtest.framework) – product environment variables can be added in Xcode by navigating to Product -> Scheme -> Edit Scheme -> Run (in left column) -> Arguments -> Environment Variables

7. Navigate to Server and open Server.xcodeproj in Xcode

8. Under Build Settings -> Search Paths -> Header Search Paths, add path to ClientServerShared directory

9. You can now build and run ClientServerTest* and Server from Xcode or command line

\* Note: To run client server tests from command line DYLD_FRAMEWORK_PATH environment variable needs to be set to be set to path to directory containing gtest.framework

## Other Notes

* Although this project is for use as a client server file system, it can easily be adapted to different server implementations (ServerBackend) and wire protocols. All that is required is to implement the ServerBackend  functions outlined in ServerDispatcher.h and create two tuple types and regular expressions corresponding to the request and response format of the desired wire protocol.

* The server currently does not support editing or deletion of files nor creation of directories.
