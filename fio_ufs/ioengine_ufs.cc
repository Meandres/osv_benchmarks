/**
 * @file
 * @brief UNVMe fio plugin engine.
 */

#include <osv/ucache.hh>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>


//#include "config-host.h"
#include "fio.h"
#include "optgroup.h"       // since fio 2.4
typedef struct {
    struct io_u**       io_reqs;
    unsigned int iocq_count;
    unsigned int iocq_size;
} fio_thread;

ucache::VMA* vma = NULL;

/*
 * The ->event() hook is called to match an event number with an io_u.
 * After the core has called ->getevents() and it has returned eg 3,
 * the ->event() hook must return the 3 events that have completed for
 * subsequent calls to ->event() with [0-2]. Required.
 */
static struct io_u* fio_ufs_event(struct thread_data *td, int event)
{
    fio_thread* ft = (fio_thread*)td->io_ops_data;
    return ft->io_reqs[event];
}

/*
 * The ->getevents() hook is used to reap completion events from an async
 * io engine. It returns the number of completed events since the last call,
 * which may then be retrieved by calling the ->event() hook with the event
 * numbers. Required.
 */
static int fio_ufs_getevents(struct thread_data *td, unsigned int min,
                               unsigned int max, const struct timespec *t)
{
    fio_thread* ft = (fio_thread*)td->io_ops_data;
    struct timespec t0, t1;
    int events = 0;
    uint64_t timeout = 0;

    if (t) {
        timeout = t->tv_sec * 1000000000L + t->tv_nsec;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    }


    uint64_t pos = 0;
    for (;;) {
        if(ft->io_reqs[pos] == NULL){
            pos++;
            continue;
        }
        assert(ft->io_reqs[pos] != NULL);
        
        ucache::aio_req_t* req = (ucache::aio_req_t*)ft->io_reqs[pos]->engine_data;
        assert(req->bios != NULL);
        
        vma->file->poll(req);
        events++;
        delete req;
        ft->io_reqs[pos]->engine_data = NULL;
        //printf("finishing: %p\n", ft->io_reqs[pos]);
        pos++;

        if(events >= min){
            return events;  
        }else if (t) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
            uint64_t elapse = ((t1.tv_sec - t0.tv_sec) * 1000000000L)
                              + t1.tv_nsec - t0.tv_nsec;
            if (elapse > timeout) break;
        }
    }

    return events;
}

/*
 * The ->queue() hook is responsible for initiating io on the io_u
 * being passed in. If the io engine is a synchronous one, io may complete
 * before ->queue() returns. Required.
 *
 * The io engine must transfer in the direction noted by io_u->ddir
 * to the buffer pointed to by io_u->xfer_buf for as many bytes as
 * io_u->xfer_buflen. Residual data count may be set in io_u->resid
 * for a short read/write.
 */
static enum fio_q_status fio_ufs_queue(struct thread_data *td, struct io_u *io_u)
{
    /*
     * Double sanity check to catch errant write on a readonly setup
     */
    fio_ro_check(td, io_u);
    assert((io_u->flags & IO_U_F_FLIGHT) != 0);
    //printf("queueing: %p\n", io_u);

    fio_thread* ft = (fio_thread*)td->io_ops_data;
    assert(ft->io_reqs != NULL);
    assert((io_u->offset % (unsigned long long)vma->pageSize) == 0ull);
    ucache::aio_req_t* ret = NULL;

    switch (io_u->ddir) {
    case DDIR_READ:
        ret = vma->file->aread(vma->start+io_u->offset, io_u->offset, io_u->buflen, true);
        break;

    case DDIR_WRITE:
        ret = vma->file->awrite(vma->start+io_u->offset, io_u->offset, io_u->buflen, true);
        break;

    default:
        break;
    }

    assert((io_u->flags & IO_U_F_FLIGHT) != 0);
    io_u->engine_data = (void*)ret;
    //assert(ft->iocq_count < ft->iocq_size);
    //assert(ft->io_reqs[ft->iocq_count] == NULL || ft->io_reqs[ft->iocq_count]->engine_data == NULL);
    ft->io_reqs[ft->iocq_count] = io_u;
    ft->iocq_count = (ft->iocq_count + 1)%ft->iocq_size;
    assert((io_u->flags & IO_U_F_FLIGHT) != 0);
	  return FIO_Q_QUEUED;
}

/*
 * Hook for opening the given file. Unless the engine has special
 * needs, it usually just provides generic_file_open() as the handler.
 */
static int fio_ufs_open(struct thread_data *td, struct fio_file *f)
{
    if(ucache::uCacheManager->totalPhysSize == 0){
        uint64_t physgb = 16ul*1024*1024*1024;
        if(getenv("PHYSGB"))
            physgb = atof(getenv("PHYSGB"))*1024*1024*1024;
        ucache::createCache(physgb, 64);
    }
    if(vma == NULL){
        vma = ucache::uCacheManager->mmap(td->o.filename, td->o.size, td->o.bs[0]);
        for(uint64_t i = 0; i < vma->buffers.size(); i++){ // pre-fault all pages
            ucache::uCacheManager->handleFault(vma, vma->buffers[i], true);
        }
    }
    return 0;
}

static int fio_ufs_close(struct thread_data *td, struct fio_file *f){
    return 0;
}

static int fio_ufs_get_file_size(struct thread_data *td, struct fio_file *f){
    if(ucache::uCacheManager->totalPhysSize == 0){
        uint64_t physgb = 16ul*1024*1024*1024;
        if(getenv("PHYSGB"))
            physgb = atof(getenv("PHYSGB"))*1024*1024*1024;
        ucache::createCache(physgb, 64);
    }
    if(vma == NULL){
        vma = ucache::uCacheManager->mmap(td->o.filename, td->o.size, td->o.bs[0]);
        for(uint64_t i = 0; i < vma->buffers.size(); i++){ // pre-fault all pages
            ucache::uCacheManager->handleFault(vma, vma->buffers[i], true);
        }
    }
    if(fio_file_size_known(f)){
        return 0;
    }
    f->real_file_size = vma->size;
    fio_file_set_size_known(f);
    return 0;
}

static int fio_ufs_setup(struct thread_data *td)
{ 
    fio_thread* ft = (fio_thread*)calloc(1, sizeof(fio_thread*));
    assert(ft != NULL);

    td->io_ops_data = ft;
    ft->iocq_size = td->o.iodepth;
    ft->iocq_count = 0;
    ft->io_reqs = (struct io_u**)malloc(ft->iocq_size * sizeof(struct io_u*));
    assert(ft->io_reqs != NULL);
    for(int i = 0; i<ft->iocq_size; i++){
        ft->io_reqs[i] = NULL;
    }

    return 0;
}



/*
 * The ->io_u_init() function is called once for each queue depth entry
 * (numjobs x iodepth) prior to .init and after .get_file_size.
 * It is needed if io_u buffer needs to be remapped.
 */

static int fio_unvme_io_u_init(struct thread_data *td, struct io_u *io_u)
{
    /*u64 sizeToAllocate =  io_u->buflen;
    void* buf = zeroInitVM(sizeToAllocate);
    Buffer buffer(buf, sizeToAllocate);
    buffer.map(ymap_getPage(computeOrder(sizeToAllocate)));
    unvme_io_u_t *unvme_io_u = (unvme_io_u_t*) malloc(sizeof(unvme_io_u_t));
    if (!unvme_io_u) {
        printf("error: unvme_alloc\n");
        return 1;
    }
    unvme_io_u->io_u = io_u;
    unvme_io_u->buf = buf;
    unvme_io_u->buflen = sizeToAllocate;*/
    io_u->engine_data = NULL;

    return 0;
}

/*
 * The ->io_u_free() function is called once for each queue depth entry
 * (numjobs x iodepth) prior to .init and after .get_file_size.
 * It is needed if io_u buffer needs to be remapped.
 */
/*
static void fio_unvme_io_u_free(struct thread_data *td, struct io_u *io_u)
{
    unvme_io_u_t* unvme_io_u = (unvme_io_u_t*)io_u->engine_data;
    if (unvme_io_u) {
        assert(unvme_io_u->io_u == io_u);
        //unvme_free(unvme.ns, unvme_io_u->buf);
        Buffer buffer(unvme_io_u->buf, unvme_io_u->buflen);
        u64 phys = buffer.unmap();
        ymap_putPage(phys, computeOrder(unvme_io_u->buflen));
        munmap(unvme_io_u->buf, unvme_io_u->buflen);
	      free(unvme_io_u);
        io_u->engine_data = NULL;
    }
}
*/

// Note that the structure is exported, so that fio can get it via
// dlsym(..., "ioengine");
struct ioengine_ops ioengine = {
    .name               = "ufs_fio",
    .version            = FIO_IOOPS_VERSION,
    .flags              = FIO_NOEXTEND | FIO_ASYNCIO_SYNC_TRIM | FIO_NOEXTEND | FIO_NODISKUTIL | FIO_DISKLESSIO | FIO_MEMALIGN,
    .setup               = fio_ufs_setup,
    .queue              = fio_ufs_queue,
    .getevents          = fio_ufs_getevents,
    .event              = fio_ufs_event,
    .open_file          = fio_ufs_open,
    .close_file         = fio_ufs_close,
    .get_file_size      = fio_ufs_get_file_size,
    //.io_u_init          = fio_unvme_io_u_init,
    //.io_u_free          = fio_unvme_io_u_free,
};

static void fio_init fio_ufs_register(void){
    register_ioengine(&ioengine);
}

static void fio_exit fio_ufs_unregister(void){
    unregister_ioengine(&ioengine);
}

#ifdef __cplusplus
}
#endif
