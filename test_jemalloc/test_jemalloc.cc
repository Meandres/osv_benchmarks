#include <jemalloc.h>


int main(){
	int* buf = (int*)je_malloc(sizeof(int));
	je_free(buf);
}
