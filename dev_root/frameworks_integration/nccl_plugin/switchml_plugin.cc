/**
 * SwitchML Project
 * @file switchml_plugin.cc
 * @brief The switchml nccl plugin.
 */

#include <nccl.h>
#include <nccl_net.h>

#include <string>

#include "switchml/context.h"
#include "utils.h"

#define __hidden __attribute__((visibility("hidden")))

using namespace std;

#define MAX_REQUESTS 128

ncclNet_t NCCL_PLUGIN_SYMBOL = {NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL};

struct SwitchMLListenComm {
  int dev;
  void* p2p_listen_comm;
};

struct SwitchMLCollComm {
  int rank;
  int nranks;
  void* sendcomm;
  void* recvcomm;
};

struct SwitchMLMemHandle{
  int  type;
  void *p2p_mh;
};

struct SwitchMLRequest {
  ncclDataType_t dataType;
  void* sendData;
  void* recvData;
  int count;
  std::shared_ptr<switchml::Job> switchml_job_ref;
};

ncclDebugLogger_t logger;
switchml::Context* ctx_ptr = 0;

static __inline__ bool TypeCheck(ncclDataType_t type) {
  switch (type) {
    case ncclInt32:
      return true;
    // We support uint8 since pytorch uses this type to check flags across workers. Refer to https://github.com/pytorch/pytorch/issues/24137.
    case ncclUint8: 
      return true;
    //case ncclUint32:
    //  return true;
    case ncclFloat32:
      return true;
    default:
      return false;
  }
}

static __inline__ bool OpCheck(ncclRedOp_t op) {
  switch (op) {
    case ncclSum:
      return true;
    default:
      return false;
  }
}

static __inline__ int TypeSize(ncclDataType_t type) {
  switch (type) {
    case ncclInt8:
      return sizeof(int8_t);
    case ncclUint8:
      return sizeof(uint8_t);
    case ncclFloat16:
      return 2;
    case ncclInt32:
      return sizeof(int32_t);
    case ncclUint32:
      return sizeof(uint32_t);
    case ncclFloat32:
      return sizeof(float);
    case ncclInt64:
      return sizeof(int64_t);
    case ncclUint64:
      return sizeof(uint64_t);
    case ncclFloat64:
      return sizeof(double);
    default:
      return -1;
  }
}

/**
 *  Initialize the collective network.
 */
__hidden ncclResult_t ncclSwitchMLInit(ncclDebugLogger_t log_function) {
  logger = log_function;
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLInit");
  ctx_ptr = &(switchml::Context::GetInstance());
  ctx_ptr->Start();
  return NCCL_PLUGIN_SYMBOL.init(log_function);
}

/**
 * Return the number of adapters capable of doing collective operations.
 * If ndev returns 0, all other functions might be set to NULL.
 */
__hidden ncclResult_t ncclSwitchMLDevices(int* ndev) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLDevices");
  *ndev = 1;
  return ncclSuccess;
}

/**
 * Get various device properties.
 */
__hidden ncclResult_t ncclSwitchMLGetProperties(int dev, ncclNetProperties_t* props) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLGetProperties dev %d", dev);

  NCCLCHECK(NCCL_PLUGIN_SYMBOL.getProperties(dev, props));

  props->ptrSupport = NCCL_PTR_HOST /* TODO | NCCL_PTR_CUDA*/;
  props->maxComms = 1;  // Maximum number of comms we can create

  return ncclSuccess;
}

/**
 * Create a receiving object and provide a handle to connect to it. The
 * handle can be up to NCCL_NET_HANDLE_MAXSIZE bytes and will be exchanged
 * between ranks to create connections.
 */
__hidden ncclResult_t ncclSwitchMLListen(int dev, void* handle, void** listen_comm) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLListen dev %d", dev);

  SwitchMLListenComm* lcomm;

  NCCLCHECK(NcclIbMalloc((void**)&lcomm, sizeof(SwitchMLListenComm)));

  ncclResult_t status = NCCL_PLUGIN_SYMBOL.listen(dev, handle, &lcomm->p2p_listen_comm);

  lcomm->dev = dev;
  *listen_comm = lcomm;
  return status;
}

/**
 * Close and free listen comm objects
 */
__hidden ncclResult_t ncclSwitchMLCloseListen(void* listen_comm) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLCloseListen");

  SwitchMLListenComm* lcomm = (SwitchMLListenComm*)listen_comm;

  ncclResult_t status = NCCL_PLUGIN_SYMBOL.closeListen(lcomm->p2p_listen_comm);

  free(lcomm);
  return status;
}

/**
 * Create a group for collective operations. handles have been created
 * using listen() above. rank indicates caller's rank in the collective network.
 * Between listen() and connect(), NCCL exchanges the handle through a side channel (sockets).
 */
__hidden ncclResult_t ncclSwitchMLConnect(void* handles[], int nranks, int rank, void* listen_comm, void** coll_comm) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLConnect rank %d nranks %d", rank, nranks);

  SwitchMLListenComm* lcomm = (SwitchMLListenComm*)listen_comm;
  SwitchMLCollComm* cc;

  NCCLCHECK(NcclIbMalloc((void**)&cc, sizeof(SwitchMLCollComm)));

  cc->nranks = nranks;
  cc->rank = rank;
  if (rank == -1) {
    WARN("Could not determine my rank\n");
    return ncclInternalError;
  }

  int next = (rank + 1) % nranks;
  NCCLCHECK(NCCL_PLUGIN_SYMBOL.connect(lcomm->dev, handles[next], &cc->sendcomm));
  NCCLCHECK(NCCL_PLUGIN_SYMBOL.accept(lcomm->p2p_listen_comm, &cc->recvcomm));

  *coll_comm = cc;
  return ncclSuccess;
}

/**
 * Close and free collective comm objects
 */
__hidden ncclResult_t ncclSwitchMLCloseColl(void* coll_comm) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLCloseColl");

  SwitchMLCollComm* cc = (SwitchMLCollComm*)coll_comm;

  NCCLCHECK(NCCL_PLUGIN_SYMBOL.closeRecv(cc->recvcomm));
  NCCLCHECK(NCCL_PLUGIN_SYMBOL.closeSend(cc->sendcomm));
  free(cc);

  return ncclSuccess;
}

/**
 * Returns whether a reduction operation on a data type is supported.
 * 1 for supported, 0 otherwise.
 */
__hidden ncclResult_t ncclSwitchMLReduceSupport(ncclDataType_t type, ncclRedOp_t op, int* supported) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLReduceSupport dataType %d redOp %d", type, op);

  *supported = TypeCheck(type) && OpCheck(op);

  return ncclSuccess;
}

/**
 * Register memory. Type is either NCCL_PTR_HOST or NCCL_PTR_CUDA.
 */
__hidden ncclResult_t ncclSwitchMLRegMr(void* coll_comm, void* data, int size, int type, void** mhandle) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLRegMr size %d type %s", size, (type == NCCL_PTR_HOST) ? "Host" : "Cuda");

  SwitchMLMemHandle* mh;

  NCCLCHECK(NcclIbMalloc((void**)&mh, sizeof(SwitchMLMemHandle)));

  mh->type = type;

  //SwitchMLCollComm* cc = (SwitchMLCollComm*)coll_comm;
  //NCCLCHECK(NCCL_PLUGIN_SYMBOL.regMr(cc->recvcomm, data, size, type, &mh->p2p_mh));

  *mhandle = mh;
  return ncclSuccess;
}

/**
 * Deregister memory.
 */
__hidden ncclResult_t ncclSwitchMLDeregMr(void* coll_comm, void* mhandle) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLDeregMr");

  SwitchMLMemHandle* mh = (SwitchMLMemHandle*) mhandle;

  //SwitchMLCollComm* cc = (SwitchMLCollComm*)coll_comm;
  //NCCLCHECK(NCCL_PLUGIN_SYMBOL.deregMr(cc->recvcomm, mh->p2p_mh));

  free(mh);
  return ncclSuccess;
}

/**
 * Performs an asynchronous allreduce operation on the collective group.
 * May return request == NULL if the call cannot be performed (or would block).
 */
__hidden ncclResult_t ncclSwitchMLIallreduce(void* collComm, void* sendData, void* recvData, int count,
                                             ncclDataType_t dataType, ncclRedOp_t redOp, void* sendMhandle, void* recvMhandle, void** request) {

  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLIallreduce count %d dataType %d redOp %d", count, dataType, redOp);

  SwitchMLMemHandle* smh = (SwitchMLMemHandle*) sendMhandle;
  SwitchMLMemHandle* rmh = (SwitchMLMemHandle*) recvMhandle;

  // Nccl to Switchml mappings
  switchml::DataType switchml_datatype;
  switchml::AllReduceOperation switchml_op;
  switch (dataType) {
    case ncclUint8:
    case ncclInt32:
      switchml_datatype = switchml::DataType::INT32;
      break;
    case ncclFloat32:
      switchml_datatype = switchml::DataType::FLOAT32;
      break;
    default:
      return ncclInvalidArgument;
  }
  switch (redOp) {
    case ncclSum:
      switchml_op = switchml::AllReduceOperation::SUM;
      break;
    default:
      return ncclInvalidArgument;
  }

  SwitchMLRequest* smlr = new SwitchMLRequest();
  smlr->dataType = dataType;
  smlr->sendData = sendData;
  smlr->recvData = recvData;
  smlr->count = count;
  if(dataType == ncclUint8) {
    // Switchml does not really support uint8 so we create a new temp int32 array to store this and send it to switchml.
    uint8_t* uint8_send_data = (uint8_t*) sendData;
    int32_t* int32_send_data = new int32_t[count];
    for(int i = 0; i < count; i++) {
      int32_send_data[i] = uint8_send_data[i];
    }
    sendData = int32_send_data;
    recvData = int32_send_data;
  }
  smlr->switchml_job_ref = ctx_ptr->AllReduceAsync(sendData, recvData, count, switchml_datatype, switchml_op);
  *request = smlr;

  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLIallreduce submitted idx %d numel %d",  smlr->switchml_job_ref->id_,  smlr->switchml_job_ref->tensor_.numel);
  return ncclSuccess;
}

/**
 * Perform a flush/fence to make sure all data received with NCCL_PTR_CUDA is
 * visible to the GPU
 */
__hidden ncclResult_t ncclSwitchMLFlush(void* coll_comm, void* data, int size, void* mhandle) {
  TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLFlush size %d", size);
  
  SwitchMLCollComm *cc = (struct SwitchMLCollComm*)coll_comm;

  return NCCL_PLUGIN_SYMBOL.flush(cc->recvcomm, data, size, mhandle);
}

/**
 * Test whether a request is complete. If size is not NULL, it returns the
 * number of bytes sent/received.
 */
__hidden ncclResult_t ncclSwitchMLTest(void* request, int* done, int* size) {

  SwitchMLRequest* smlr = (SwitchMLRequest*) request;
  
  if (smlr->switchml_job_ref->GetJobStatus() == switchml::JobStatus::FINISHED) {
    // size needs only to be set when the request is complete.
    *size = smlr->count * TypeSize(smlr->dataType);
    *done = 1;

    // If it was a ncclUint8 then we've allocated a temporary array that we need to unload and deallocate.
    if(smlr->dataType == ncclUint8) {
      uint8_t* uint8_recv_data = (uint8_t*) smlr->recvData;
      int32_t* int32_recv_data = (int32_t*) smlr->switchml_job_ref->tensor_.out_ptr;
      for(int i = 0; i < smlr->count; i++) {
        uint8_recv_data[i] = int32_recv_data[i];
      }
      delete int32_recv_data;
    }
    TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLTest job id %d numel %d data_type %d finished !!", smlr->switchml_job_ref->id_, smlr->switchml_job_ref->tensor_.numel, smlr->switchml_job_ref->tensor_.data_type);
    delete smlr;
  } else {
    *done = 0;
    TRACE(NCCL_INIT | NCCL_NET, "ncclSwitchMLTest job id %d numel %d data_type %d still running", smlr->switchml_job_ref->id_, smlr->switchml_job_ref->tensor_.numel, smlr->switchml_job_ref->tensor_.data_type);
  }

  return ncclSuccess;
}

ncclCollNet_t NCCL_COLLNET_PLUGIN_SYMBOL = {"SWITCHMLv1",
                                            ncclSwitchMLInit,
                                            ncclSwitchMLDevices,
                                            ncclSwitchMLGetProperties,
                                            ncclSwitchMLListen,
                                            ncclSwitchMLConnect,
                                            ncclSwitchMLReduceSupport,
                                            ncclSwitchMLRegMr,
                                            ncclSwitchMLDeregMr,
                                            ncclSwitchMLIallreduce,
                                            ncclSwitchMLFlush,
                                            ncclSwitchMLTest,
                                            ncclSwitchMLCloseColl,
                                            ncclSwitchMLCloseListen};
