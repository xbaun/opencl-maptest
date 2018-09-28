#include <iostream>
#include <CL/cl.h>
#include <string.h>
#include <thread>
#include <chrono>

using namespace std;

#define CHECK_CLRET_VAL(name, retexec) (check_clr_status_beg((char*)name),          check_clr_status_end((char*)name, ret = (retexec)))
#define CHECK_CLRET_REF(name, refexec) (check_clr_status_beg((char*)name), refexec, check_clr_status_end((char*)name, ret))

void check_clr_status_beg(char* name);
void check_clr_status_end(char* name, cl_int ret);
cl_device_id select_device(char* device_name, cl_uint* num_platforms, cl_uint* num_devices);
char* readKernel();
const char *getErrorString(cl_int error);

cl_device_id device_id = NULL;

typedef void(*cl_callback)(cl_event, cl_int, void *);

int main() {
    
    cl_int ret;

    cl_uint num_devices;
    cl_uint num_platforms;

    cl_context context = NULL;
    cl_command_queue command_queue1 = NULL;
    cl_command_queue command_queue2 = NULL;
    cl_program program = NULL;
    cl_kernel kernel1 = NULL;

    device_id = select_device((char*) "PowerVR", &num_platforms, &num_devices);

    CHECK_CLRET_REF("clCreateContext", context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret));
    CHECK_CLRET_REF("clCreateCommandQueue1", command_queue1 = clCreateCommandQueue(context, device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE/* | CL_QUEUE_PROFILING_ENABLE*/, &ret));
    CHECK_CLRET_REF("clCreateCommandQueue2", command_queue2 = clCreateCommandQueue(context, device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE/* | CL_QUEUE_PROFILING_ENABLE*/, &ret));

    char* kernel_source = readKernel();
    CHECK_CLRET_REF("clCreateProgramWithSource", program = clCreateProgramWithSource(context, 1, (const char **) &kernel_source, NULL, &ret));
    CHECK_CLRET_VAL("clBuildProgram", clBuildProgram(program, num_devices, &device_id, NULL, NULL, NULL));
    free(kernel_source);

    if (ret == CL_BUILD_PROGRAM_FAILURE) {
        
        size_t log_size;
        
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char *) malloc(log_size);
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf((char*)"%s\n", log);

        free(log);
        return 1;
    }

    cl_mem memobj_exit;
    cl_mem memobj_resl;

    CHECK_CLRET_REF("clCreateKernel1", kernel1 = clCreateKernel(program, "foo", &ret));

    CHECK_CLRET_REF("clCreateBuffer: memobj_exit", memobj_exit = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, 1 * sizeof (unsigned int), NULL, &ret));
    CHECK_CLRET_REF("clCreateBuffer: memobj_resl", memobj_resl = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, 1 * sizeof (unsigned int), NULL, &ret));

    CHECK_CLRET_VAL("clSetKernelArg 0: memobj_exit", clSetKernelArg(kernel1, 0, sizeof (cl_mem), (const void *) &memobj_exit));
    CHECK_CLRET_VAL("clSetKernelArg 1: memobj_resl", clSetKernelArg(kernel1, 1, sizeof (cl_mem), (const void *) &memobj_resl));
    
    
    unsigned int *arg_exit = NULL;
    unsigned int *arg_resl = NULL;
    
    CHECK_CLRET_REF("clEnqueueMapBuffer: arg_exit", arg_exit = (unsigned int*) clEnqueueMapBuffer(command_queue2, memobj_exit, CL_TRUE, CL_MAP_WRITE, 0, sizeof (unsigned int), 0, NULL, NULL, &ret));
    CHECK_CLRET_REF("clEnqueueMapBuffer: arg_resl", arg_resl = (unsigned int*) clEnqueueMapBuffer(command_queue2, memobj_resl, CL_TRUE, CL_MAP_WRITE, 0, sizeof (unsigned int), 0, NULL, NULL, &ret));
    
    *arg_exit = 0;
    *arg_resl = 0;
    
    CHECK_CLRET_VAL("clEnqueueUnmapMemObject: arg_exit", clEnqueueUnmapMemObject(command_queue2, memobj_exit, arg_exit, 0, NULL, NULL));
    CHECK_CLRET_VAL("clEnqueueUnmapMemObject: arg_resl", clEnqueueUnmapMemObject(command_queue2, memobj_exit, arg_resl, 0, NULL, NULL));
  
    size_t global_item_size[1] = {32};
    size_t local_item_size [1] = {32};

    std::thread tfoo([&]() {
        printf("Host: BEG - Kernel Foo\n");
        CHECK_CLRET_VAL("clEnqueueNDRangeKernel Foo", clEnqueueNDRangeKernel(command_queue1, kernel1, 1, NULL, global_item_size, local_item_size, 0, NULL, NULL));
        printf("Host: END - Kernel Foo\n");
    });

    printf("wait 2s to exit kernel ...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    printf("wait 2s to exit kernel ... ok\n");

    while(!*arg_resl) {
        
        printf("\nvvvvvvvvvvvvvv\n");
        printf("arg_exit=%u, *arg_exit=%p\n", *arg_exit, arg_exit);
                
        CHECK_CLRET_REF("clEnqueueMapBuffer: arg_exit", arg_exit = (unsigned int*) clEnqueueMapBuffer(command_queue2, memobj_exit, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, sizeof (unsigned int), 0, NULL, NULL, &ret));
        CHECK_CLRET_REF("clEnqueueMapBuffer: arg_resl", arg_resl = (unsigned int*) clEnqueueMapBuffer(command_queue2, memobj_resl, CL_TRUE, CL_MAP_READ , 0, sizeof (unsigned int), 0, NULL, NULL, &ret));

        printf("arg_exit=%u, *arg_exit=%p\n", *arg_exit, arg_exit);
        
        *arg_exit = 1;

        printf("arg_resl=%u\n", *arg_resl);
        
        CHECK_CLRET_VAL("clEnqueueUnmapMemObject: arg_exit", clEnqueueUnmapMemObject(command_queue2, memobj_exit, arg_exit, 0, NULL, NULL));
        CHECK_CLRET_VAL("clEnqueueUnmapMemObject: arg_resl", clEnqueueUnmapMemObject(command_queue2, memobj_resl, arg_resl, 0, NULL, NULL));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    CHECK_CLRET_VAL("clReleaseMemObject: memobj_exit", clReleaseMemObject(memobj_exit));
    CHECK_CLRET_VAL("clReleaseMemObject: memobj_resl", clReleaseMemObject(memobj_resl));

    CHECK_CLRET_VAL("clReleaseCommandQueue: command_queue1", clReleaseCommandQueue(command_queue1));
    CHECK_CLRET_VAL("clReleaseCommandQueue: command_queue2", clReleaseCommandQueue(command_queue2));

    printf("arg_resl=%u\n", *arg_resl);

    tfoo.join();

    {
        printf("Exit?");
        getchar();
    }

    return 0;
}

void check_clr_status_beg(char* name) {
    printf("%s... ", name);
    std::cout << std::flush;
}

void check_clr_status_end(char* name, cl_int ret) {
    printf("%s\n", getErrorString(ret));
}

cl_device_id select_device(char* device_name, cl_uint* num_platforms, cl_uint* num_devices) {

    cl_platform_id* platform_ids = NULL;
    cl_device_id** device_ids = NULL;
    cl_device_id device_id = NULL;

    char msgbuf[1000];

    clGetPlatformIDs(0, NULL, num_platforms);

    platform_ids = new cl_platform_id[*num_platforms];
    device_ids = new cl_device_id*[*num_platforms];
    clGetPlatformIDs(*num_platforms, platform_ids, NULL);

    for (cl_uint i = 0; i < *num_platforms; i++) {

        char platform_string[1024];
        clGetPlatformInfo(platform_ids[i], CL_PLATFORM_NAME, sizeof (platform_string), &platform_string, NULL);
        printf("+ Platform: %s\n", platform_string);

        clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VERSION, sizeof (platform_string), &platform_string, NULL);
        printf("|           %s\n", platform_string);

        clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, 0, NULL, num_devices);
        device_ids[i] = new cl_device_id[*num_devices]; //(cl_device_id*)malloc(sizeof(cl_device_id) * deviceCount);
        clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, *num_devices, device_ids[i], NULL);


        for (cl_uint j = 0; j < *num_devices; j++) {
            char device_string[1024];

            clGetDeviceInfo(device_ids[i][j], CL_DEVICE_NAME, sizeof (device_string), &device_string, NULL);
            printf("*   Device: %s\n", device_string);

            cl_uint max_device_max_compute_units;
            clGetDeviceInfo(device_ids[i][j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof (max_device_max_compute_units), &max_device_max_compute_units, NULL);
            printf("*   CL_DEVICE_MAX_COMPUTE_UNITS: %u\n", max_device_max_compute_units);
    

            size_t max_work_group_size;
            clGetDeviceInfo(device_ids[i][j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof (max_work_group_size), &max_work_group_size, NULL);
            printf("*   CL_DEVICE_MAX_WORK_GROUP_SIZE: %zu\n", max_work_group_size);
    

            cl_uint max_work_item_dimensions;
            clGetDeviceInfo(device_ids[i][j], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof (max_work_item_dimensions), &max_work_item_dimensions, NULL);
            printf("*   CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: %u\n", max_work_item_dimensions);
    

            size_t *max_work_items_sizes = (size_t *) malloc(max_work_item_dimensions * sizeof (size_t));
            clGetDeviceInfo(device_ids[i][j], CL_DEVICE_MAX_WORK_ITEM_SIZES, max_work_item_dimensions * sizeof (size_t), max_work_items_sizes, NULL);
            printf("*   CL_DEVICE_MAX_WORK_ITEM_SIZES: [");
    

            for (cl_uint k = 0; k < max_work_item_dimensions; k++) {
                printf("%ld", max_work_items_sizes[k]);
        
                if (k < max_work_item_dimensions - 1)
                    printf(", ");
            }

            printf("]\n");
            free(max_work_items_sizes);

            if (device_id == NULL && strstr(device_string, device_name)) {
                device_id = device_ids[i][j];
                printf("    ^^^^^^\n");
        
            }
        }
    }

    return device_id;
}

char* readKernel() {

    FILE *fp = fopen("./Kernel.cl", "rb+");

    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }

    char *source_str;
    size_t source_size;

    fseek(fp, 0, SEEK_END);
    source_size = ftell(fp);
    rewind(fp);

    source_str = (char*) malloc(source_size * sizeof (char) + 1);
    source_str[source_size] = '\0';

    fread(source_str, 1, source_size, fp);
    fclose(fp);

    return source_str;
}

const char *getErrorString(cl_int error) {
    switch (error) {
            // run-time and JIT compiler errors
        case 0: return "CL_SUCCESS";
        case -1: return "CL_DEVICE_NOT_FOUND";
        case -2: return "CL_DEVICE_NOT_AVAILABLE";
        case -3: return "CL_COMPILER_NOT_AVAILABLE";
        case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case -5: return "CL_OUT_OF_RESOURCES";
        case -6: return "CL_OUT_OF_HOST_MEMORY";
        case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case -8: return "CL_MEM_COPY_OVERLAP";
        case -9: return "CL_IMAGE_FORMAT_MISMATCH";
        case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case -11: return "CL_BUILD_PROGRAM_FAILURE";
        case -12: return "CL_MAP_FAILURE";
        case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case -15: return "CL_COMPILE_PROGRAM_FAILURE";
        case -16: return "CL_LINKER_NOT_AVAILABLE";
        case -17: return "CL_LINK_PROGRAM_FAILURE";
        case -18: return "CL_DEVICE_PARTITION_FAILED";
        case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

            // compile-time errors
        case -30: return "CL_INVALID_VALUE";
        case -31: return "CL_INVALID_DEVICE_TYPE";
        case -32: return "CL_INVALID_PLATFORM";
        case -33: return "CL_INVALID_DEVICE";
        case -34: return "CL_INVALID_CONTEXT";
        case -35: return "CL_INVALID_QUEUE_PROPERTIES";
        case -36: return "CL_INVALID_COMMAND_QUEUE";
        case -37: return "CL_INVALID_HOST_PTR";
        case -38: return "CL_INVALID_MEM_OBJECT";
        case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case -40: return "CL_INVALID_IMAGE_SIZE";
        case -41: return "CL_INVALID_SAMPLER";
        case -42: return "CL_INVALID_BINARY";
        case -43: return "CL_INVALID_BUILD_OPTIONS";
        case -44: return "CL_INVALID_PROGRAM";
        case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case -46: return "CL_INVALID_KERNEL_NAME";
        case -47: return "CL_INVALID_KERNEL_DEFINITION";
        case -48: return "CL_INVALID_KERNEL";
        case -49: return "CL_INVALID_ARG_INDEX";
        case -50: return "CL_INVALID_ARG_VALUE";
        case -51: return "CL_INVALID_ARG_SIZE";
        case -52: return "CL_INVALID_KERNEL_ARGS";
        case -53: return "CL_INVALID_WORK_DIMENSION";
        case -54: return "CL_INVALID_WORK_GROUP_SIZE";
        case -55: return "CL_INVALID_WORK_ITEM_SIZE";
        case -56: return "CL_INVALID_GLOBAL_OFFSET";
        case -57: return "CL_INVALID_EVENT_WAIT_LIST";
        case -58: return "CL_INVALID_EVENT";
        case -59: return "CL_INVALID_OPERATION";
        case -60: return "CL_INVALID_GL_OBJECT";
        case -61: return "CL_INVALID_BUFFER_SIZE";
        case -62: return "CL_INVALID_MIP_LEVEL";
        case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
        case -64: return "CL_INVALID_PROPERTY";
        case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
        case -66: return "CL_INVALID_COMPILER_OPTIONS";
        case -67: return "CL_INVALID_LINKER_OPTIONS";
        case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

            // extension errors
        case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
        case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
        case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
        case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
        case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
        case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
        default: return "Unknown OpenCL error";
    }
}