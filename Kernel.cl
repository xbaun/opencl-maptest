__kernel void foo(__global unsigned int *exit, __global unsigned int *resl) {

	printf("###### foo: beg -> exit=%u\n", *exit);
        printf("max %u\n", UINT_MAX);

	unsigned int count = 0;

	for (;;) {

		count++; 

		//mem_fence(CLK_GLOBAL_MEM_FENCE);
		//prefetch(r, 1);
		
                //printf("count=%u", count);
                barrier(CLK_GLOBAL_MEM_FENCE);
                //printf("count=%u", count);

		if (*exit) {
			printf("break exit=%u, count=%u\n", *exit, count);
			break;
		}
	}

	*resl = count;

	printf("###### foo: end -> exit=%u with resl=%u, count=%u\n", *exit, *resl, count);
}