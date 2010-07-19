#include <assert.h>
#include <sys/types.h>

#include "gasnet_safe.h"

#include "tlsf.h"                /* symmetric var allocation       */

#include "state.h"
#include "barrier.h"
#include "warn.h"

#include "shmem.h"

/*
 * Tony's development interface
 */

static gasnet_seginfo_t* seginfo_table;

void
__symmetric_memory_init(void)
{
  seginfo_table = (gasnet_seginfo_t *)calloc(__state.numpes,
                                             sizeof(gasnet_seginfo_t));
  assert(seginfo_table != (gasnet_seginfo_t *)NULL);

  GASNET_SAFE( gasnet_getSegmentInfo(seginfo_table, __state.numpes) );

  __gasnet_barrier_all();

  /*
   * each PE initializes its own table, but can see addresses of all PEs
   */
  init_memory_pool(seginfo_table[__state.mype].size,
                   seginfo_table[__state.mype].addr);
  __gasnet_barrier_all();
}

/*
 * where the symmetric memory starts on the given PE
 */
void *
__symmetric_var_base(int pe)
{
  return seginfo_table[pe].addr;
}

/*
 * is the address in the managed symmetric area?
 */
int
__symmetric_var_in_range(void *addr, int pe)
{
  void *top = seginfo_table[pe].addr + seginfo_table[pe].size;
  return (seginfo_table[pe].addr <= addr) && (addr < top) ? 1 : 0;
}

/*
 * PUBLIC INTERFACE
 */

long malloc_error = 0;         /* exposed for error codes */

const long SHMEM_MALLOC_OK=0L;
const long SHMEM_MALLOC_FAIL=1L;
const long SHMEM_MALLOC_ALREADY_FREE=2L;

void *
shmalloc(size_t size)
{
  void *area = __symmetric_var_base(__state.mype);
  /*
   * internally use a long pointer to align nicely.
   * shfree() *could* check alignment before freeing
   */
  long *addr;

  addr = (long *)malloc_ex(size, area);

  if (addr == (long *)NULL) {
    __shmem_warn("NOTICE", "shmalloc(%ld) failed\n", size);
    malloc_error = SHMEM_MALLOC_FAIL;
  }

  // __shmem_warn("INFO", "allocating %ld @ %p", size, addr);

  return (void *)addr;
}

void
shfree(void *addr)
{
  void *area = __symmetric_var_base(__state.mype);

  if (addr == (void *)NULL) {
    __shmem_warn("NOTICE", "shfree(%p) already null\n", addr);
    malloc_error = SHMEM_MALLOC_ALREADY_FREE;
    return;
  }

  // __shmem_warn("INFO", "freeing @ %p", addr);

  free_ex(addr, area);

  malloc_error = 0;
}

void *
shrealloc(void *addr, size_t size)
{
  void *area = __symmetric_var_base(__state.mype);
  void *newaddr;

  if (addr == (void *)NULL) {
    __shmem_warn("NOTICE", "shrealloc(%p) null\n", addr);
    malloc_error = SHMEM_MALLOC_ALREADY_FREE;
    return (void *)NULL;
  }

  newaddr = realloc_ex(addr, size, area);

  malloc_error = 0;

  return newaddr;
}

/*
 * TODO: shmemalign, this is probably wrong
 *
 * The shmemalign function allocates a block in the symmetric heap that
 * has a byte alignment specified by the alignment argument.
 */
void *
shmemalign(size_t alignment, size_t size)
{
  return shmalloc(size);
}
