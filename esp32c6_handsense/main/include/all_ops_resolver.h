#ifndef TENSORFLOW_LITE_MICRO_ALL_OPS_RESOLVER_H_
#define TENSORFLOW_LITE_MICRO_ALL_OPS_RESOLVER_H_

#include "tensorflow/lite/micro/compatibility.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

namespace tflite
{

    // The magic number in the template parameter is the maximum number of ops that
    // can be added to AllOpsResolver. It can be increased if needed. And most
    // applications that care about the memory footprint will want to directly use
    // MicroMutableOpResolver and have an application specific template parameter.
    // The examples directory has sample code for this.
    class AllOpsResolver : public MicroMutableOpResolver<128>
    {
    public:
        AllOpsResolver();

    private:
        TF_LITE_REMOVE_VIRTUAL_DELETE
    };

} // namespace tflite

#endif // TENSORFLOW_LITE_MICRO_ALL_OPS_RESOLVER_H_