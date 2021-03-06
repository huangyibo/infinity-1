#ifndef R_RDMAIMPL_HPP_
#define R_RDMAIMPL_HPP_

#include <infinity/core/Context.h>
#include <infinity/memory/Buffer.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/queues/QueuePairFactory.h>
#include <infinity/requests/RequestToken.h>
using namespace infinity;

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "fuckhust.hpp"

/////////////////////// current time
#include <string>
#include <stdio.h>
#include <time.h>

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
inline const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}
//////////////////// current time end

#ifdef RDEBUG
#define rdma_debug std::cerr << currentDateTime() << "|RdmaNative Debug: "
#else
#define rdma_debug                                                                                                             \
    if (false)                                                                                                                 \
    std::cerr
#endif
#define rdma_error std::cerr << currentDateTime() << "|RdmaNative: "

#ifndef uint32_t
#define uint32_t unsigned int
#endif
#ifndef uint64_t
#define uint64_t unsigned long long
#endif

#define INIT_BUFFER_SIZE 4096

template <typename T> inline void checkedDelete(T *&ptr) {
    if (ptr)
        delete ptr;
    ptr = nullptr;
}

extern core::Context *context;
extern queues::QueuePairFactory *qpFactory;

#if HUST
#define magic_t uint32_t
enum {
#else
enum magic_t : uint32_t {
#endif
    MAGIC_QUERY_BUFFER_READY = 0xffffffff,
    MAGIC_QUERY_WRITTEN = 0xaaaaaaaa,
    MAGIC_QUERY_READ = 0xaaaa5555,
    MAGIC_RESPONSE_WRITTEN = 0x55555555,
    MAGIC_RESPONSE_READ = 0x5555aaaa
};
#if HUST
namespace std {
extern std::string to_string(magic_t &m);
extern std::string to_string(int &m);
}
#endif
struct ServerStatusType {
    magic_t queryMagic;
    magic_t responseMagic;
    volatile uint64_t currentQueryLength;
    uint64_t currentResponseLength;
    memory::RegionToken dynamicQueryBufferToken;
    memory::RegionToken dynamicResponseBufferToken;
};
typedef memory::RegionToken DynamicBufferTokenBufferTokenType;

class CRdmaServerConnectionInfo {
  private:
    queues::QueuePair *pQP; // must delete

    memory::Buffer *pDynamicBufferTokenBuffer;           // must delete
    memory::RegionToken *pDynamicBufferTokenBufferToken; // must delete
#define pServerStatus ((ServerStatusType *)pDynamicBufferTokenBuffer->getData())

    memory::Buffer *pDynamicQueryBuffer;           // must delete
    memory::Buffer *pDynamicResponseBuffer;           // must delete
    uint64_t currentQueryBufferSize;                        // Default 4K
    uint64_t currentResponseBufferSize;                        // Default 4K

    void initFixedLocalBuffer() {
        pDynamicBufferTokenBuffer = new memory::Buffer(context, sizeof(ServerStatusType));
        pDynamicBufferTokenBufferToken = pDynamicBufferTokenBuffer->createRegionToken();
        pDynamicResponseBuffer = new memory::Buffer(context, currentResponseBufferSize);
        pDynamicResponseBuffer->createRegionTokenAt(&pServerStatus->dynamicResponseBufferToken);
        pDynamicQueryBuffer = new memory::Buffer(context, currentQueryBufferSize);
        pDynamicQueryBuffer->createRegionTokenAt(&pServerStatus->dynamicQueryBufferToken);
        pServerStatus->queryMagic = MAGIC_QUERY_READ;
        pServerStatus->responseMagic = MAGIC_RESPONSE_READ;
        pServerStatus->currentQueryLength = 0;
        pServerStatus->currentResponseLength = 0;
    }

public:
    CRdmaServerConnectionInfo() : pQP(nullptr), pDynamicQueryBuffer(nullptr), 
    pDynamicResponseBuffer(nullptr),
    pDynamicBufferTokenBuffer(nullptr), pDynamicBufferTokenBufferToken(nullptr), 
    currentQueryBufferSize(INIT_BUFFER_SIZE), currentResponseBufferSize(INIT_BUFFER_SIZE) {

    }
    ~CRdmaServerConnectionInfo() {
        checkedDelete(pQP);
        checkedDelete(pDynamicQueryBuffer);
        checkedDelete(pDynamicResponseBuffer);
        checkedDelete(pDynamicBufferTokenBuffer);
    }

    void waitAndAccept() {
        initFixedLocalBuffer();
        pQP = qpFactory->acceptIncomingConnection(pDynamicBufferTokenBufferToken, sizeof(DynamicBufferTokenBufferTokenType));
    }

    std::string getClientIp() {
        return pQP->peerAddr;
    }

    bool canReadQuery() {
        // Use this chance to check if client has told server its query size and allocate buffer.
        if (pServerStatus->queryMagic == MAGIC_QUERY_READ && pServerStatus->currentQueryLength != 0) {
        // Warning: pServerStatus->currentQueryLength may be under editing! The read value maybe broken!
        // So I set currentQueryLength as volatile and read it again.
        broken_value_read_again:
            uint64_t queryLength = pServerStatus->currentQueryLength;
            if (queryLength != pServerStatus->currentQueryLength)
                goto broken_value_read_again;

            if (queryLength > currentQueryBufferSize) {
                rdma_debug << "allocating new buffer rather than reusing old one... new length " << queryLength << std::endl;
                checkedDelete(pDynamicQueryBuffer);
                pDynamicQueryBuffer = new memory::Buffer(context, queryLength);
                pDynamicQueryBuffer->createRegionTokenAt(&pServerStatus->dynamicQueryBufferToken);
                //pDynamicBuffer->resize(queryLength);
                currentQueryBufferSize = queryLength;
            }
            pServerStatus->queryMagic = MAGIC_QUERY_BUFFER_READY;
        }
        return pServerStatus->queryMagic == MAGIC_QUERY_WRITTEN;
    }
    bool canWriteResponse() {
        return pServerStatus->responseMagic == MAGIC_RESPONSE_READ;
    }

    void readQuery(void *&dataPtr, uint64_t &dataSize) {
        rdma_debug << "SERVER " << (long)this << ": readQuery called" << std::endl;
        if (pServerStatus->queryMagic != MAGIC_QUERY_WRITTEN)
            throw std::runtime_error("Query is not readable while calling readQuery");
        dataPtr = pDynamicQueryBuffer->getData();
        dataSize = pServerStatus->currentQueryLength;
        pServerStatus->queryMagic = MAGIC_QUERY_READ;
        pServerStatus->currentQueryLength = 0;
    }

    void writeResponse(const void *dataPtr, uint64_t dataSize) {
        rdma_debug << "SERVER " << (long)this << ": writeResponse called. dataPtr is " << (long)dataPtr << ",dataSize is " << dataSize << std::endl;
        if (!canWriteResponse())
            throw std::runtime_error("previous response is not read");
        if(dataSize > currentResponseBufferSize) {
            checkedDelete(pDynamicResponseBuffer);
            pDynamicResponseBuffer = new memory::Buffer(context, dataSize);
            //pDynamicBuffer->resize(dataSize);
            pDynamicResponseBuffer->createRegionTokenAt(&pServerStatus->dynamicResponseBufferToken);
            currentResponseBufferSize = dataSize;
        }
        // TODO: here's an extra copy. use dataPtr directly and jni global reference to avoid it!
        pServerStatus->currentResponseLength = dataSize;
        std::memcpy(pDynamicResponseBuffer->getData(), dataPtr, dataSize);

        //if (pServerStatus->responseMagic != MAGIC_RESPONSE_READ)
        //    throw std::runtime_error("write response: magic is changed while copying memory data.");
        pServerStatus->responseMagic = MAGIC_RESPONSE_WRITTEN;
    }
};

class CRdmaClientConnectionInfo {
    queues::QueuePair *pQP;                                    // must delete
    memory::RegionToken *pRemoteDynamicBufferTokenBufferToken; // must not delete
    uint64_t remoteQueryBufferCurrentSize; // If this query is smaller than last, do not wait for the server to allocate space.

    void rdmaSetServerCurrentQueryLength(uint64_t queryLength) {
        requests::RequestToken reqToken(context);
        memory::Buffer queryLenBuffer(context, sizeof(uint64_t));
        *(uint64_t *)queryLenBuffer.getData() = queryLength;
        pQP->write(&queryLenBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, currentQueryLength),sizeof(uint64_t), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
    }
    uint64_t rdmaGetServerCurrentResponseLength() {
        memory::Buffer serverResponseLenBuffer(context, sizeof(uint64_t));
        requests::RequestToken reqToken(context);
        pQP->read(&serverResponseLenBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, currentResponseLength), sizeof(uint64_t), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
        return *(uint64_t *)serverResponseLenBuffer.getData();
    }
    void rdmaSetQueryMagic(magic_t magic) {
        requests::RequestToken reqToken(context);
        memory::Buffer serverMagicBuffer(context, sizeof(magic_t));
        *(magic_t *)serverMagicBuffer.getData() = magic;
#if !HUST
        static_assert(offsetof(ServerStatusType, queryMagic) == 0, "Use read with more arg if offsetof(magic) is not 0.");
#endif
        pQP->write(&serverMagicBuffer, pRemoteDynamicBufferTokenBufferToken, sizeof(magic_t), &reqToken);
        reqToken.waitUntilCompleted();
    }
    void rdmaSetResponseMagic(magic_t magic) {
        requests::RequestToken reqToken(context);
        memory::Buffer serverMagicBuffer(context, sizeof(magic_t));
        *(magic_t *)serverMagicBuffer.getData() = magic;
        pQP->write(&serverMagicBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, responseMagic), sizeof(magic_t), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
    }

    magic_t rdmaGetQueryMagic() {
        memory::Buffer serverMagicBuffer(context, sizeof(magic_t));
        requests::RequestToken reqToken(context);
        pQP->read(&serverMagicBuffer, pRemoteDynamicBufferTokenBufferToken, sizeof(magic_t), &reqToken);
        reqToken.waitUntilCompleted();
        return *(magic_t *)serverMagicBuffer.getData();
    }

    magic_t rdmaGetResponseMagic() {
        memory::Buffer serverMagicBuffer(context, sizeof(magic_t));
        requests::RequestToken reqToken(context);
        pQP->read(&serverMagicBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, responseMagic), sizeof(magic_t), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
        return *(magic_t *)serverMagicBuffer.getData();
    }

    std::string jAddrToAddr(const char *jAddr) {
        // inode112/10.10.0.112:16020
        std::string tmp(jAddr);
        size_t slashLoc = tmp.find('/');
        if(slashLoc == std::string::npos)
            return tmp; // not jaddr
        size_t colonLoc = tmp.find(':');
        if(colonLoc == std::string::npos)
            throw std::invalid_argument(std::string("illegal jAddr `") + jAddr + "`. Example: host/1.2.3.4:1080");
        std::string realAddr = tmp.substr(slashLoc+1, colonLoc-slashLoc-1);
        return realAddr;
    }

  public:
    CRdmaClientConnectionInfo() : pQP(nullptr), pRemoteDynamicBufferTokenBufferToken(nullptr),
    remoteQueryBufferCurrentSize(INIT_BUFFER_SIZE) {

    }
    ~CRdmaClientConnectionInfo() { checkedDelete(pQP); }

    void connectToRemote(const char *serverAddr, int serverPort) {
        pQP = qpFactory->connectToRemoteHost(jAddrToAddr(serverAddr).c_str(), serverPort);
        pRemoteDynamicBufferTokenBufferToken = reinterpret_cast<memory::RegionToken *>(pQP->getUserData());
    }

    void writeQuery(void *dataPtr, uint64_t dataSize) {
        rdma_debug << "CLIENT " << (long)this << ": writeQuery called. dataSize is " << dataSize << std::endl;
        memory::Buffer wrappedDataBuffer(context, dataPtr, dataSize);
        memory::Buffer wrappedSizeBuffer(context, &dataSize, sizeof(dataSize));
#if !HUST
#if __cplusplus < 201100L
        static_assert(std::is_pod<ServerStatusType>::value == true, "ServerStatusType must be pod to use C offsetof.");
#else
        static_assert(std::is_standard_layout<ServerStatusType>::value == true,
                      "ServerStatusType must be standard layout in cxx11 to use C offsetof.");
#endif
#endif
        requests::RequestToken reqToken(context);
        // write data size.
        pQP->write(&wrappedSizeBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, currentQueryLength),
                   sizeof(dataSize), queues::OperationFlags(), &reqToken);

        // Wait for the server allocating buffer...
        if (dataSize > remoteQueryBufferCurrentSize) {
            rdma_debug << "server is allocating new buffer rather than reusing old one..." << std::endl;
            while (true) {
                static_assert(offsetof(ServerStatusType, queryMagic) == 0, "Use read with more arg if offsetof(magic) is not 0.");
                if (MAGIC_QUERY_BUFFER_READY == rdmaGetQueryMagic())
                    break; // Remote buffer is ready. Fire!
            }
            remoteQueryBufferCurrentSize = dataSize;
        }
        reqToken.waitUntilCompleted(); // WARN: this is a async rdma write
        // write the real data
        memory::Buffer tempTokenBuffer(context, sizeof(ServerStatusType));
        pQP->read(&tempTokenBuffer, pRemoteDynamicBufferTokenBufferToken, &reqToken);
        reqToken.waitUntilCompleted();
        memory::RegionToken remoteDynamicBufferToken = ((ServerStatusType *)tempTokenBuffer.getData())->dynamicQueryBufferToken;
        rdma_debug << "real data write ---" << std::endl;
        // workaround for buffer to large unknown bug(at most 2048 byte per read/write):
        for(size_t cter = 0; cter < dataSize / 2048; ++cter) {
            pQP->write(&wrappedDataBuffer, cter*2048, &remoteDynamicBufferToken, cter*2048, 2048, queues::OperationFlags(), &reqToken);
        }
        if(dataSize % 2048 != 0)
            pQP->write(&wrappedDataBuffer, dataSize/2048*2048, &remoteDynamicBufferToken, dataSize/2048*2048, dataSize%2048, queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
        rdma_debug << "real data write done ---" << std::endl;

        rdmaSetQueryMagic(MAGIC_QUERY_WRITTEN);
    }

    bool canWriteQuery() { return rdmaGetQueryMagic() == MAGIC_QUERY_READ; }
    bool canReadResponse() { return rdmaGetResponseMagic() == MAGIC_RESPONSE_WRITTEN; }

    void readResponse(memory::Buffer *&pResponseDataBuf) {
        rdma_debug << "CLIENT " << (long)this << ": readResponse called." << std::endl;
        // Undefined behavior if the response is not ready.
        requests::RequestToken reqToken(context);
        memory::Buffer tempTokenBuffer(context, sizeof(ServerStatusType));
        pQP->read(&tempTokenBuffer, pRemoteDynamicBufferTokenBufferToken, &reqToken);
        reqToken.waitUntilCompleted();
        memory::RegionToken remoteDynamicBufferToken = ((ServerStatusType *)tempTokenBuffer.getData())->dynamicResponseBufferToken;
        uint64_t realResponseLen = rdmaGetServerCurrentResponseLength();
        memory::Buffer *pResponseData = new memory::Buffer(context, realResponseLen);
        // workaround for buffer to large unknown bug(at most 2048 byte per read/write):
        //pQP->read(pResponseData, &remoteDynamicBufferToken, realResponseLen, &reqToken);
        for(size_t cter = 0; cter < realResponseLen / 2048; ++cter) {
            pQP->read(pResponseData, cter*2048, &remoteDynamicBufferToken, cter*2048, 2048, queues::OperationFlags(), &reqToken);
        }
        if(realResponseLen % 2048 != 0)
            pQP->read(pResponseData, realResponseLen/2048*2048, &remoteDynamicBufferToken,  realResponseLen/2048*2048, realResponseLen%2048, queues::OperationFlags(), &reqToken);
 
        rdmaSetResponseMagic(MAGIC_RESPONSE_READ);

        pResponseDataBuf = pResponseData;
        // WARNING: You must delete the pResponseDataBuf after using it!!!
    }
};

#endif
