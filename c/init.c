/* initialize.c - initproc */

#include <i386.h>
#include <xeroskernel.h>
#include <xeroslib.h>

#ifdef TESTING
#include <xerostest.h>
#endif

extern	int	entry( void );  /* start of kernel image, use &start    */
extern	int	end( void );    /* end of kernel image, use &end        */
extern  long	freemem; 	/* start of free memory (set in i386.c) */
extern char	*maxaddr;	/* max memory address (set in i386.c)	*/

/************************************************************************/
/***				NOTE:				      ***/
/***								      ***/
/***   This is where the system begins after the C environment has    ***/
/***   been established.  Interrupts are initially DISABLED.  The     ***/
/***   interrupt table has been initialized with a default handler    ***/
/***								      ***/
/***								      ***/
/************************************************************************/

/*------------------------------------------------------------------------
 *  The init process, this is where it all begins...
 *------------------------------------------------------------------------
 */
void initproc( void )				/* The beginning */
{
  kprintf( "\n\nCPSC 415, 2016W1 \n32 Bit Xeros 0.01 \nLocated at: %x to %x\n", 
	   &entry, &end); 

  // kernel will crash if this fails
  ASSERT_EQUAL(DEFAULT_STACK_SIZE % 16, 0);

  /* Initialize kernel */

  kmeminit();
  kprintf("kmem initialized\n");
  
  di_init_devtable();
  kprintf("devices initialized\n");

  ctsw_init_evec();
  kprintf("context switcher initialized\n");

  dispinit();
  kprintf("dispatcher initialized\n");
  

/************************************************************************/
/* This section must be manually commented/uncommented to select        */
/* the desired test to run                                              */
/************************************************************************/
#ifdef TESTING
  // We can't test our memory management or queues in another process,
  // due to their critical nature
  //mem_run_all_tests();
  //disp_run_all_tests();

  // Other tests should be dispatched
  //dispatch(&syscall_run_all_tests);
  //dispatch(&copyinout_run_all_tests);
  //dispatch(&msg_run_all_tests);
  //dispatch(&timer_run_all_tests);
  //dispatch(&signal_run_all_tests);
  setEnabledKbd(1);
  dispatch(&dev_run_all_tests);
#else
  // enable pre-emption
  initPIT(1000 / TICK_LENGTH_IN_MS);

  // Launch the root process
  dispatch(&parent);
#endif


  kprintf("\n\nIf you see this, something went horribly wrong!\n");
  kprintf("Pretend this message never appeared and casually powercycle the VM...\n");
  for(;;) ; /* loop forever */
}
