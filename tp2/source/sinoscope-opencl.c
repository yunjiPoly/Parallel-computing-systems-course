#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include "log.h"
#include "sinoscope.h"
#include "opencl.h"

typedef struct sinoscope_float_args {
	float interval_inverse;
    float time;
    float max;
    float phase0;
    float phase1;
    float dx;
    float dy;
} sinoscope_float_args_t;

typedef struct sinoscope_int_args {
	unsigned int buffer_size;
    unsigned int width;
    unsigned int height;
    unsigned int taylor;
    unsigned int interval;
} sinoscope_int_args_t;

int sinoscope_opencl_init(sinoscope_opencl_t* opencl, cl_device_id opencl_device_id, unsigned int width,
			  unsigned int height) {
	cl_int error = 0;
	opencl->device_id = opencl_device_id;

	opencl->context = clCreateContext(0,1, &opencl_device_id, NULL, NULL, &error);
	if(error!= CL_SUCCESS) { 
		printf("error clCreateContext");
		return -1;
	}

	opencl->queue = clCreateCommandQueue(opencl->context, opencl_device_id, 0 , &error);
	if(error!= CL_SUCCESS) { 
		printf("error clCreateCommandQueue");
		return -1;
	}
	
	opencl->buffer = clCreateBuffer(opencl->context,CL_MEM_WRITE_ONLY, width * height * 3, NULL, &error);
	if(error!= CL_SUCCESS) { 
		printf("error clCreateBuffer");
		return -1;
	}
	char* src;
	size_t size;

	opencl_load_kernel_code(&src, &size);

	cl_program pgm = clCreateProgramWithSource(opencl->context, 1, (const char**) &src,&size, &error);
	if(error!= CL_SUCCESS) { 
		printf("error clCreateProgramWithSource");
		return -1;
	}
	error = clBuildProgram(pgm, 1,&opencl_device_id, "-I "__OPENCL_INCLUDE__, NULL, NULL);
	if(error!= CL_SUCCESS) { 
		printf("error clBuildProgram");
		return -1;
	}
	opencl->kernel = clCreateKernel(pgm, "kernel_sinoscope",&error);
	if(error!= CL_SUCCESS) { 
		printf("error clCreateKernel");
		return -1;
	}
	return 0;
}

void sinoscope_opencl_cleanup(sinoscope_opencl_t* opencl)
{
	if(opencl->kernel) clReleaseKernel(opencl->kernel);
	if(opencl->queue) clReleaseCommandQueue(opencl->queue);
	if(opencl->context) clReleaseContext(opencl->context);
	if(opencl->buffer) clReleaseMemObject(opencl->buffer);
}

int sinoscope_image_opencl(sinoscope_t* sinoscope) {
	if (sinoscope == NULL) {
        LOG_ERROR_NULL_PTR();
        goto fail_exit;
    }

	cl_int error;

	sinoscope_float_args_t float_args = {
		sinoscope->interval_inverse,
    	sinoscope->time,
    	sinoscope->max,
    	sinoscope->phase0,
    	sinoscope->phase1,
    	sinoscope->dx,
    	sinoscope->dy
	};

	sinoscope_int_args_t int_args = {
		sinoscope->buffer_size,
    	sinoscope->width,
    	sinoscope->height,
    	sinoscope->taylor,
    	sinoscope->interval
	};

	error = clSetKernelArg(sinoscope->opencl->kernel, 0, sizeof(cl_mem), &(sinoscope->opencl->buffer));
	if(error!= CL_SUCCESS) { 
		printf("error clSetKernelArg");
		return -1;
	}
	error |= clSetKernelArg(sinoscope->opencl->kernel, 1, sizeof(sinoscope_int_args_t), &(int_args));
	if(error!= CL_SUCCESS) { 
		printf("error clSetKernelArg");
		return -1;
	}
	error |= clSetKernelArg(sinoscope->opencl->kernel, 2, sizeof(sinoscope_float_args_t), &(float_args));
	if(error!= CL_SUCCESS) { 
		printf("error clSetKernelArg");
		return -1;
	}
	
	const size_t global_size[]  = {int_args.width, int_args.height};

	error = clEnqueueNDRangeKernel(sinoscope->opencl->queue, sinoscope->opencl->kernel, 2, NULL, global_size, 0, 0, NULL, NULL);
	if(error!= CL_SUCCESS) { 
		printf("error clEnqueueNDRangeKernel");
		return -1;
	}

	clEnqueueReadBuffer(sinoscope->opencl->queue, sinoscope->opencl->buffer, CL_TRUE, 0, sinoscope->buffer_size, sinoscope->buffer, 0, NULL, NULL);
	if(error!= CL_SUCCESS) { 
		printf("error clEnqueueReadBuffer");
		return -1;
	}
	return 0;

fail_exit:
    return -1;
}
